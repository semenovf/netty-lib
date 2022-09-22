////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019 - 2022 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2021.11.06 Initial version.
//      2022.08.15 Changed signatures for startup/cleanup.
////////////////////////////////////////////////////////////////////////////////
#include "newlib/udt.hpp"
#include "pfs/netty/error.hpp"
#include "pfs/netty/p2p/udt/sockets_api.hpp"
#include "pfs/i18n.hpp"
#include "pfs/log.hpp"

namespace netty {
namespace p2p {
namespace udt {

std::atomic<int> sockets_api::_instance_count {0};

static std::int32_t const DEFAULT_LISTENER_BACKLOG {64};
static std::chrono::milliseconds const DEFAULT_POLL_INTERVAL {10};

sockets_api::sockets_api ()
{
    _opts.listener_backlog = DEFAULT_LISTENER_BACKLOG;
    _opts.poll_interval = DEFAULT_POLL_INTERVAL;

    if (_instance_count == 0) {
        try {
            UDT::startup_context ctx;

            ctx.state_changed_callback = [this] (socket_id sid) {
                _socket_state_changed_buffer.push(sid);
            };

            UDT::startup(std::move(ctx));
        } catch (CUDTException ex) {
            // Here may be exception CUDTException(1, 0, WSAGetLastError());
            // related to WSAStartup call.
            if (CUDTException{1, 0, 0}.getErrorCode() == ex.getErrorCode()) {
                throw error{make_error_code(errc::system_error)};
            } else {
                throw error{make_error_code(errc::unexpected_error)};
            }
        }
    }

    ++_instance_count;
}

sockets_api::~sockets_api ()
{
    for (auto & itpos: _index_by_socket_id) {
        auto pos = itpos.second;
        pos->close();
    }

    _index_by_socket_id.clear();
    _sockets.clear();

    if (--_instance_count == 0) {
        // No exception expected
        UDT::cleanup();
    }
}

bool sockets_api::set_option (option_enum opttype, std::intmax_t value)
{
    switch (opttype) {
        case option_enum::listener_backlog:
            if (value > 0 && value <= (std::numeric_limits<std::int32_t>::max)()) {
                _opts.listener_backlog = static_cast<std::int32_t>(value);
                return true;
            }

            log_error(tr::_("Bad listener backlog"));
            break;

        default:
            break;
    }

    return false;
}

bool sockets_api::set_option (option_enum opttype, socket4_addr sa)
{
    switch (opttype) {
        case option_enum::listener_address:
            _opts.listener_address = sa;
            return true;
        default:
            break;
    }

    return false;
}

bool sockets_api::set_option (option_enum opttype, std::chrono::milliseconds msecs)
{
    switch (opttype) {
        case option_enum::poll_interval:
            _opts.poll_interval = msecs;
            return true;

        default:
            break;
    }

    return false;
}

udp_socket * sockets_api::locate (socket_id sid) const
{
    auto pos = _index_by_socket_id.find(sid);

    if (pos == _index_by_socket_id.end())
        return nullptr;

    return & *pos->second;
}

sockets_api::socket_id sockets_api::add_socket (udp_socket && s)
{
    auto sid = s.native();

    auto pos = _sockets.insert(_sockets.end(), std::move(s));
    auto res = _index_by_socket_id.insert({sid, pos});

    if (!res.second) {
        throw error{
              make_error_code(errc::engine_error)
            , tr::f_("Add socket failure with id: {}", sid)
        };
    }

    return sid;
}

sockets_api::socket_id sockets_api::listen ()
{
    socket_type listener;
    listener.bind(_opts.listener_address);
    listener.listen(_opts.listener_backlog);
    _poller.add(listener);

    LOG_TRACE_2("Default listener: {}. Status: {}"
        , listener
        , listener.state_string());

    auto listener_options = listener.dump_options();

    for (auto const & opt_item: listener_options) {
        LOG_TRACE_3("   * {}: {}", opt_item.first, opt_item.second);
    }

    return add_socket(std::move(listener));
}

sockets_api::socket_id sockets_api::connect (inet4_addr addr, std::uint16_t port)
{
    socket_type sock;
    sock.connect(addr, port);
    auto sid = add_socket(std::move(sock));
    auto pos = _connecting_sockets.insert(sid);
    PFS__ASSERT(pos.second, "");
    return sid;
}

void sockets_api::poll (std::chrono::milliseconds interval)
{
    auto rc = _poller.wait(interval);

    // Some events rised
    if (rc > 0) {
        _poller.process_events(
              [this] (socket_id sid) { process_poll_input_event(sid); }
            , [this] (socket_id sid) { process_poll_output_event(sid); }
        );
    }
}

void sockets_api::process_poll_input_event (socket_id sid)
{
    auto * sock = locate(sid);

    if (sock) {
        if (sock->state() == udp_socket::LISTENING) {
            // Accept socket (for UDT backend see udt/api.cpp:440)
            process_acceptance(sock);
        } else if (sock->state() == udp_socket::CONNECTED) {
            if (ready_read)
                ready_read(sid, sock);
        }
    }
}

void sockets_api::process_poll_output_event (socket_id sid)
{
    auto * sock = locate(sid);

    if (sock) {
        if (sock->state() == udp_socket::LISTENING) {
            // There is no significant output events for listener (not yet).
            ;
        } else if (sock->state() == udp_socket::CONNECTED) {
            ;
        }
    }
}

void sockets_api::process_acceptance (socket_type * plistener)
{
    auto sock = plistener->accept();
    _poller.add(sock, poller_type::POLL_IN_EVENT | poller_type::POLL_ERR_EVENT);

    LOG_TRACE_2("Socket accepted: {}", sock);

    auto options = sock.dump_options();

    for (auto const & opt_item: options) {
        (void)opt_item;
        LOG_TRACE_3("   * {}: {}", opt_item.first, opt_item.second);
    }

    socket_accepted(sock.native(), sock.saddr());
    add_socket(std::move(sock));
}

void sockets_api::process_connected (socket_type * psock)
{
    LOG_TRACE_2("Socket connected to: {}", *psock);

    auto options = psock->dump_options();

    for (auto const & opt_item: options) {
        (void)opt_item;
        LOG_TRACE_3("   * {}: {}", opt_item.first, opt_item.second);
    }

    socket_connected(psock->native(), psock->saddr());
    _poller.add(*psock, poller_type::POLL_IN_EVENT | poller_type::POLL_ERR_EVENT);
}

void sockets_api::process_sockets_state_changed ()
{
    if (!_socket_state_changed_buffer.empty()) {
        socket_id sid;

        if (socket_state_changed) {
            while (_socket_state_changed_buffer.try_pop(sid)) {
                auto pos = _index_by_socket_id.find(sid);

                if (pos != _index_by_socket_id.end()) {
                    socket_type * psock = & *pos->second;
                    socket_state_changed(psock);

                    auto state = psock->state();

                    if (state == udp_socket::CLOSED)
                        socket_closed(sid, psock->saddr());

                    switch (state) {
                        case udp_socket::CONNECTING:
                            LOG_TRACE_2("Connecting in progress to: {}", *psock);
                            break;
                        case udp_socket::CONNECTED: {
                            auto pos1 = _connecting_sockets.find(sid);
                            bool was_connecting = (pos1 != _connecting_sockets.end());

                            if (was_connecting) {
                                LOG_TRACE_2("Connected to: {}", *psock);

                                process_connected(psock);
                                _connecting_sockets.erase(pos1);
                            }

                            break;
                        }

                        case udp_socket::CLOSED:
                        case udp_socket::BROKEN:
                        case udp_socket::NONEXIST: {
                            _sockets.erase(pos->second);
                            _index_by_socket_id.erase(pos);

                            auto pos1 = _connecting_sockets.find(sid);

                            if (pos1 != _connecting_sockets.end())
                                _connecting_sockets.erase(pos1);

                            break;
                        }

                        default:
                            break;
                    }
                }
            }
        }
    }
}

void sockets_api::loop ()
{
    poll(_opts.poll_interval);
    process_sockets_state_changed();
}

}}} // namespace netty::p2p::udt
