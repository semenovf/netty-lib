////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.09 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "../connecting_poller.hpp"

#if NETTY__UDT_ENABLED
#   include "newlib/udt.hpp"
#   include "pfs/netty/udt/epoll_poller.hpp"
#endif

#include "pfs/log.hpp"

static char const * TAG = "UDT";

namespace netty {

#if NETTY__UDT_ENABLED

template<>
void connecting_poller<udt::epoll_poller>::add (native_socket_type sock, error * perr)
{
    error err;

    _rep.add(sock, & err);

    if (err) {
        if (perr) {
            *perr = std::move(err);
            return;
        } else {
            throw err;
        }
    }

    auto status = UDT::getsockstate(sock);

    // Need to observe connecting process and catch connection refused
    if (status == CONNECTING) {
        // Use UDT_EXP_MAX_COUNTER option to tune connection refused interval
        std::uint64_t exp_threshold = 0; // microseconds
        int exp_threshold_size = sizeof(exp_threshold);
        auto rc = UDT::getsockopt(sock, 0, UDT_EXP_THRESHOLD, & exp_threshold, & exp_threshold_size);

        if (rc == UDT::ERROR) {
            error err {
                make_error_code(errc::poller_error)
                , tr::_("UDT get socket option failure")
                , UDT::getlasterror_desc()
            };

            if (perr) {
                *perr = std::move(err);
                return;
            } else {
                throw err;
            }
        }

        auto exp_timepoint = future_timepoint(std::chrono::milliseconds{exp_threshold / 1000});
        _connecting_sockets.emplace(sock, exp_timepoint);

//         LOGD(TAG, "exp_threshold: {}, current: {}, future: {}"
//             , exp_threshold, current_timepoint().time_since_epoch()
//             , exp_timepoint.time_since_epoch());
    }
}

template <>
int connecting_poller<udt::epoll_poller>::poll (std::chrono::milliseconds millis, error * perr)
{
    auto n = _rep.poll(_rep.eid, nullptr, & _rep.writefds, millis, perr);

    if (n < 0)
        return n;

    if (n > 0) {
        if (!_rep.writefds.empty()) {
            for (UDTSOCKET u: _rep.writefds) {
                auto status = UDT::getsockstate(u);
                LOGD(TAG, "UDT connected socket state: {} ({})"
                    , status, status == CONNECTED ? "CONNECTED" : "?");

                remove(u);

                _connecting_sockets.erase(u);

                if (connected)
                    connected(u);
            }
        }
    }

    // Check expired connecting sockets
    if (!_connecting_sockets.empty()) {
        auto pos  = _connecting_sockets.begin();
        auto last = _connecting_sockets.end();

        while (pos != last) {
            if (timepoint_expired(pos->second)) {
                auto u = pos->first;

                remove(u);

                pos = _connecting_sockets.erase(pos);

                if (connection_refused)
                    connection_refused(u);
            } else {
                ++pos;
            }
        }
    }

    return n;
}

#endif // NETTY__UDT_ENABLED

#if NETTY__UDT_ENABLED
template class connecting_poller<udt::epoll_poller>;
#endif

} // namespace netty
