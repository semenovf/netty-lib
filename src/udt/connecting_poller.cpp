////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2023-2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.09 Initial version.
//      2024.07.29 Fixed `connecting_poller` specialization.
////////////////////////////////////////////////////////////////////////////////
#include "../connecting_poller_impl.hpp"
#include "newlib/udt.hpp"
#include "netty/trace.hpp"
#include "netty/udt/epoll_poller.hpp"

NETTY__NAMESPACE_BEGIN

template <>
connecting_poller<udt::epoll_poller>::connecting_poller ()
    : _rep(new udt::epoll_poller(false, true))
{}

template<>
void connecting_poller<udt::epoll_poller>::add (socket_id sock, error * perr)
{
    error err;

    _rep->add_socket(sock, & err);

    if (err) {
        pfs::throw_or(perr, std::move(err));
        return;
    }

    auto status = UDT::getsockstate(sock);

    // Need to observe connecting process and catch connection refused
    if (status == CONNECTING) {
        // Use UDT_EXP_MAX_COUNTER option to tune connection refused interval
        std::uint64_t exp_threshold = 0; // microseconds
        int exp_threshold_size = sizeof(exp_threshold);
        auto rc = UDT::getsockopt(sock, 0, UDT_EXP_THRESHOLD, & exp_threshold, & exp_threshold_size);

        if (rc == UDT::ERROR) {
            pfs::throw_or(perr, error {tr::f_("UDT get socket option failure: {}", UDT::getlasterror_desc())});
            return;
        }

        auto exp_timepoint = future_timepoint(std::chrono::milliseconds{exp_threshold / 1000});
        _rep->_connecting_sockets.emplace(sock, exp_timepoint);
    }
}

template <>
int connecting_poller<udt::epoll_poller>::poll (std::chrono::milliseconds millis, error * perr)
{
    auto n = _rep->poll(_rep->eid, nullptr, & _rep->writefds, millis, perr);

    if (n < 0)
        return n;

    int res = 0;

    if (n > 0) {
        if (!_rep->writefds.empty()) {
            for (UDTSOCKET u: _rep->writefds) {
                auto state = UDT::getsockstate(u);

                NETTY__TRACE("UDT", "Socket CONNECTED: sock={}; state={} ({})"
                    , u
                    , static_cast<int>(state)
                    , state == CONNECTED ? tr::_("CONNECTED") : "?");

                _rep->_connecting_sockets.erase(u);

                res++;

                if (connected)
                    connected(u);
            }
        }
    }

    // Check expiration timeout for connecting sockets
    if (!_rep->_connecting_sockets.empty()) {
        auto pos  = _rep->_connecting_sockets.begin();
        auto last = _rep->_connecting_sockets.end();

        while (pos != last) {
            if (timepoint_expired(pos->second)) {
                auto u = pos->first;

                pos = _rep->_connecting_sockets.erase(pos);

                if (connection_refused)
                    connection_refused(u, connection_failure_reason::other);
            } else {
                ++pos;
            }
        }
    }

    return res;
}

template class connecting_poller<udt::epoll_poller>;

NETTY__NAMESPACE_END
