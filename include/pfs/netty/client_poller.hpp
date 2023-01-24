////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.10 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "chrono.hpp"
#include "error.hpp"
#include "conn_status.hpp"
#include "connecting_poller.hpp"
#include "reader_poller.hpp"
#include "writer_poller.hpp"
#include "pfs/i18n.hpp"
#include <functional>
#include <utility>

namespace netty {

template <typename Backend>
class client_poller
{
public:
    using native_socket_type = typename Backend::native_socket_type;

    struct callbacks
    {
        std::function<void(native_socket_type, std::string const &)> on_error;
        std::function<void(native_socket_type)> connection_refused;
        std::function<void(native_socket_type)> connected;
        std::function<void(native_socket_type)> disconnected;
        std::function<void(native_socket_type)> ready_read;
        std::function<void(native_socket_type)> can_write;
    };

private:
    connecting_poller<Backend> _connecting_poller;
    reader_poller<Backend>     _reader_poller;
    writer_poller<Backend>     _writer_poller;

private:
    std::function<void(native_socket_type)> connected;

public:
    client_poller (callbacks && cbs)
    {
        if (cbs.on_error) {
            _connecting_poller.on_error = cbs.on_error;
            _reader_poller.on_error = cbs.on_error;
            _writer_poller.on_error = std::move(cbs.on_error);
        }

        _connecting_poller.connection_refused = std::move(cbs.connection_refused);
        connected = std::move(cbs.connected);

        _connecting_poller.connected = [this] (native_socket_type sock) {
            // sock already removed from _connecting_poller
            _reader_poller.add(sock);
            this->connected(sock);
        };

        _reader_poller.disconnected = std::move(cbs.disconnected);
        _reader_poller.ready_read   = std::move(cbs.ready_read);
        _writer_poller.can_write    = std::move(cbs.can_write);
    }

    ~client_poller () = default;

    client_poller (client_poller const &) = delete;
    client_poller & operator = (client_poller const &) = delete;
    client_poller (client_poller &&) = delete;
    client_poller & operator = (client_poller &&) = delete;

    /**
     * Add socket to connecting or regular poller according to it's connection
     * status @a cs.
     */
    template <typename Socket>
    void add (Socket const & sock, conn_status state, error * perr = nullptr)
    {
        if (state == conn_status::connecting)
            _connecting_poller.add(sock.native(), perr);
        else if (state == conn_status::connected)
            _reader_poller.add(sock, perr);
        else {
            error err {
                  make_error_code(errc::poller_error)
                , tr::_("socket must be in a connecting or connected state to be"
                    " added to the client poller")
            };

            if (perr) {
                *perr = std::move(err);
                return;
            } else {
                throw err;
            }
        }
    }

    /**
     * Remove sockets from connecting and regular pollers.
     */
    template <typename Socket>
    void remove (Socket const & sock, error * perr = nullptr)
    {
        _connecting_poller.remove(sock.native(), perr);
        _reader_poller.remove(sock.native(), perr);
        _writer_poller.remove(sock.native(), perr);
    }

    /**
     * Add socket to writer poller to waiting for socket to be able to write.
     * It will be removed automatically from writer poller.
     */
    template <typename Socket>
    void wait_for_write (Socket const & sock, error * perr = nullptr)
    {
        _writer_poller.add(sock.native(), perr);
    }

    /**
     * Check if connecting and regular pollers are empty.
     */
    bool empty () const noexcept
    {
        return _connecting_poller.empty()
            && _reader_poller.empty()
            && _writer_poller.empty() ;
    }

    /**
     * @return Total number of positive events: number of connected sockets
     *         plus number of read and write events.
     */
    int poll (std::chrono::milliseconds millis = std::chrono::milliseconds{0}
        , error * perr = nullptr)
    {
        int n1 = 0;
        int n2 = 0;
        int n3 = 0;

        auto empty_reader_poller = _reader_poller.empty();

        if (!_connecting_poller.empty()) {
            if (empty_reader_poller) {
                n1 = _connecting_poller.poll(millis, perr);
            } else {
                n1 = _connecting_poller.poll(std::chrono::milliseconds{0}, perr);
            }
        }

        if (!_writer_poller.empty())
            n2 = _writer_poller.poll(std::chrono::milliseconds{0}, perr);

        if (!empty_reader_poller)
            n3 = _reader_poller.poll(millis, perr);

        if (n1 < 0 && n2 < 0 && n3 < 0)
            return n1;

        return (n1 > 0 ? n1 : 0)
            + (n2 > 0 ? n2 : 0)
            + (n3 > 0 ? n3 : 0);
    }
};

} // namespace netty
