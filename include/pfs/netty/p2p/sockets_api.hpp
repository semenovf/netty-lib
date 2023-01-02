////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2022 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2022.12.30 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/netty/error.hpp"
#include "pfs/netty/exports.hpp"
#include "pfs/netty/socket4_addr.hpp"
#include "pfs/i18n.hpp"
#include "pfs/log.hpp"
// #include "pfs/ring_buffer.hpp"
#include <chrono>
#include <functional>
#include <list>
#include <set>
#include <unordered_map>

namespace netty {
namespace p2p {

template <typename Backend>
class sockets_api
{
public:
    using poller_type = typename Backend::poller_type;
    using socket_type = typename Backend::socket_type;
    using socket_id   = typename Backend::socket_id;

    enum option_enum: std::int16_t {
          listener_address   // socket4_addr

        // The maximum length to which the queue of pending connections
        // for listener may grow.
        , listener_backlog   // std::int32_t

        , poll_interval      // std::chrono::milliseconds
    };

private:
    using socket_container = std::list<socket_type>;
    using socket_iterator  = typename socket_container::iterator;

    struct options {
        socket4_addr listener_address;
        std::int32_t listener_backlog;
        std::chrono::milliseconds poll_interval;
    } _opts;

private:
//     Backend _rep;

    poller_type _poller;

    // All sockets (listeners / readers / writers)
    socket_container _sockets;

    // Mapping socket identified by key (native handler) to socket iterator
    // (see _sockets)
    std::unordered_map<socket_id, socket_iterator> _index_by_socket_id;

    std::set<socket_id> _connecting_sockets;

//     pfs::ring_buffer_mt<socket_id, 256> _socket_state_changed_buffer;

public:
    mutable std::function<void (std::string const &)> log_error
        = [] (std::string const &) {};

    mutable std::function<void (socket_type const *)> socket_state_changed
        = [] (socket_type const *) {};

    mutable std::function<void (socket_id, socket4_addr)> socket_accepted
        = [] (socket_id, socket4_addr) {};

    mutable std::function<void (socket_id, socket4_addr)> socket_connected
        = [] (socket_id, socket4_addr) {};

    mutable std::function<void (socket_id, socket4_addr)> socket_closed
        = [] (socket_id, socket4_addr) {};

    mutable std::function<void (socket_id, socket_type *)> ready_read
        = [] (socket_id, socket_type *) {};

private:
    sockets_api (sockets_api const &) = delete;
    sockets_api (sockets_api &&) = delete;
    sockets_api & operator = (sockets_api const &) = delete;
    sockets_api & operator = (sockets_api &&) = delete;

    socket_id add_socket (socket_type && sock)
    {
        auto sid = sock.native();

        auto pos = _sockets.insert(_sockets.end(), std::move(sock));
        auto res = _index_by_socket_id.insert({sid, pos});

        if (!res.second) {
            throw error{
                make_error_code(errc::engine_error)
                , tr::f_("add socket failure with id: {}", sid)
            };
        }

        return sid;
    }

    void poll (std::chrono::milliseconds interval)
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

    NETTY__EXPORT void process_sockets_state_changed ();
    NETTY__EXPORT void process_poll_input_event (socket_id sid);
    NETTY__EXPORT void process_poll_output_event (socket_id sid);
//     void process_acceptance (socket_type * plistener);
//     void process_connected (socket_type * psock);

public:
    NETTY__EXPORT sockets_api ();
    NETTY__EXPORT ~sockets_api ();

    /**
     * Sets boolean or integer type option.
     *
     * @return @c true on success, or @c false if option is unsuitable or
     *         value is bad.
     */
    bool set_option (option_enum opttype, std::intmax_t value)
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

    /**
     * Sets socket address type option.
     */
    bool set_option (option_enum opttype, socket4_addr sa)
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

    /**
     * Sets chrono type option.
     */
    bool set_option (option_enum opttype, std::chrono::milliseconds msecs)
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

    socket_type * locate (socket_id sid) const
    {
        auto pos = _index_by_socket_id.find(sid);

        if (pos == _index_by_socket_id.end())
            return nullptr;

        return & *pos->second;
    }

    socket_id listen ()
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

    socket_id connect (inet4_addr addr, std::uint16_t port)
    {
        socket_type sock;
        sock.connect(addr, port);
        auto sid = add_socket(std::move(sock));
        auto pos = _connecting_sockets.insert(sid);
        PFS__ASSERT(pos.second, "");
        return sid;
    }

    inline socket_id connect (socket4_addr saddr)
    {
        return connect(saddr.addr, saddr.port);
    }

    void close (socket_id sid)
    {
        auto s = locate(sid);

        if (s)
            s->close();
    }

    void loop ()
    {
        poll(_opts.poll_interval);
        process_sockets_state_changed();
    }
};

}} // namespace netty::p2p

