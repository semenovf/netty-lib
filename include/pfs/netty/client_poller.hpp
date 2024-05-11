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
#include <pfs/i18n.hpp>
#include <pfs/log.hpp>
#include <pfs/stopwatch.hpp>
#include <functional>
#include <set>
#include <utility>
#include <vector>

namespace netty {

template <typename Backend>
class client_poller
{
public:
    using native_socket_type = typename Backend::native_socket_type;

private:
    connecting_poller<Backend> _connecting_poller;
    reader_poller<Backend>     _reader_poller;
    writer_poller<Backend>     _writer_poller;
    std::vector<native_socket_type> _addable_connecting_sockets;
    std::vector<native_socket_type> _addable_readers;
    std::vector<native_socket_type> _removable_connecting_sockets;
    std::vector<native_socket_type> _removable_readers;
    std::vector<native_socket_type> _removable_writers;
    std::set<native_socket_type> _removable;

public:
    mutable std::function<void(native_socket_type, error const &)> on_failure = [] (native_socket_type, error const &) {};
    mutable std::function<void(native_socket_type)> connection_refused = [] (native_socket_type) {};
    mutable std::function<void(native_socket_type)> connected = [] (native_socket_type) {};

    /**
     * Socket has been disconnected by the peer. No disconnection required.
     */
    mutable std::function<void(native_socket_type)> disconnected = [] (native_socket_type) {};

    mutable std::function<void(native_socket_type)> ready_read = [] (native_socket_type) {};
    mutable std::function<void(native_socket_type)> can_write = [] (native_socket_type) {};

    /**
     * Socket has been removed from client poller. It can be disconnected and/or released safely.
     */
    mutable std::function<void(native_socket_type)> removed = [] (native_socket_type) {};

public:
    client_poller ()
    {
        _connecting_poller.on_failure = [this] (native_socket_type sock, error const & err) {
            // Socket must be removed from monitoring later
            _removable_connecting_sockets.push_back(sock);
            _removable.insert(sock);
            on_failure(sock, err);
        };

        _reader_poller.on_failure = [this] (native_socket_type sock, error const & err) {
            // Socket must be removed from monitoring later
            _removable_readers.push_back(sock);
            _removable.insert(sock);
            on_failure(sock, err);
        };

        _writer_poller.on_failure = [this] (native_socket_type sock, error const & err) {
            // Socket must be removed from monitoring later
            _removable_writers.push_back(sock);
            _removable.insert(sock);
            on_failure(sock, err);
        };

        _connecting_poller.connection_refused = [this] (native_socket_type sock) {
            // Socket must be removed from monitoring later
            _removable_connecting_sockets.push_back(sock);
            _removable.insert(sock);
            connection_refused(sock);
        };

        _connecting_poller.connected = [this] (native_socket_type sock) {
            // Socket must be removed from monitoring later
            _removable_connecting_sockets.push_back(sock);
            _reader_poller.add(sock); // safe to add to poller
            connected(sock);
        };

        _reader_poller.disconnected = [this] (native_socket_type sock) {
            // Socket must be removed from monitoring later
            _removable_readers.push_back(sock);
            _removable.insert(sock);
            disconnected(sock);
        };

        _reader_poller.ready_read = [this] (native_socket_type sock) {
            ready_read(sock);
        };

        _writer_poller.can_write = [this] (native_socket_type sock) {
            _removable_writers.push_back(sock);
            can_write(sock);
        };
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
        if (state == conn_status::connecting) {
            _addable_connecting_sockets.push_back(sock.native());
        } else if (state == conn_status::connected) {
            _addable_readers.push_back(sock.native());
        } else {
            pfs::throw_or(perr, error {
                  errc::poller_error
                , tr::_("socket must be in a connecting or connected state to be"
                    " added to the client poller")
            });

            return;
        }
    }

    /**
     * Remove sockets from connecting and regular pollers.
     */
    template <typename Socket>
    void remove (Socket const & sock, error * perr = nullptr)
    {
        _removable_connecting_sockets.push_back(sock);
        _removable_readers.push_back(sock);
        _removable_writers.push_back(sock);
        _removable.insert(sock);

        LOG_TRACE_3("Client socket ({}) removed from `client_poller`", to_string(sock.saddr()));
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
     * @return @c 0 if read poller is empty or poll for read is timed out.
     */
    int poll_read (std::chrono::milliseconds timeout = std::chrono::milliseconds{0}
        , error * perr = nullptr)
    {
        if (!_reader_poller.empty())
            return _reader_poller.poll(timeout, perr);

        return 0;
    }

    /**
     * @return @c 0 if write poller is empty or poll for write is timed out.
     */
    int poll_write (std::chrono::milliseconds timeout = std::chrono::milliseconds{0}
        , error * perr = nullptr)
    {
        if (!_writer_poller.empty())
            return _writer_poller.poll(timeout, perr);

        return 0;
    }

    /**
     * @return @c 0 if connecting poller is empty or poll for connection is timed out.
     */
    int poll_connected (std::chrono::milliseconds timeout = std::chrono::milliseconds{0}
        , error * perr = nullptr)
    {
        if (!_connecting_poller.empty())
            return _connecting_poller.poll(timeout, perr);

        return 0;
    }

    /**
     * @return Total number of positive events: number of connected sockets
     *         plus number of read and write events.
     */
    int poll (std::chrono::milliseconds timeout = std::chrono::milliseconds{0}
        , error * perr = nullptr)
    {
        static std::chrono::milliseconds __zero_millis{0};

        int n1 = 0;
        int n2 = 0;
        int n3 = 0;

        pfs::stopwatch<std::milli> stopwatch;

        // The order of call polls is matter

        if (!_writer_poller.empty()) {
            stopwatch.start();
            n1 = _writer_poller.poll(timeout, perr);
            stopwatch.stop();
            timeout -= std::chrono::milliseconds{stopwatch.count()};

            if (timeout < __zero_millis)
                timeout = __zero_millis;
        }

        if (!_reader_poller.empty()) {
            stopwatch.start();
            n2 = _reader_poller.poll(timeout, perr);
            stopwatch.stop();
            timeout -= std::chrono::milliseconds{stopwatch.count()};

            if (timeout < __zero_millis)
                timeout = __zero_millis;
        }

        if (!_connecting_poller.empty())
            n3 = _connecting_poller.poll(timeout, perr);

        if (!_addable_connecting_sockets.empty()) {
            for (auto sock: _addable_connecting_sockets)
                _connecting_poller.add(sock);
            _addable_connecting_sockets.clear();
        }

        if (!_addable_readers.empty()) {
            for (auto sock: _addable_readers)
                _reader_poller.add(sock, perr);
            _addable_readers.clear();
        }

        if (!_removable_connecting_sockets.empty()) {
            for (auto sock: _removable_connecting_sockets)
                _connecting_poller.remove(sock);
            _removable_connecting_sockets.clear();
        }

        if (!_removable_readers.empty()) {
            for (auto sock: _removable_readers)
                _reader_poller.remove(sock);
            _removable_readers.clear();
        }

        if (!_removable_writers.empty()) {
            for (auto sock: _removable_writers)
                _writer_poller.remove(sock);
            _removable_writers.clear();
        }

        if (!_removable.empty()) {
            for (auto sock: _removable)
                removed(sock);
            _removable.clear();
        }

        if (n1 < 0 && n2 < 0 && n3 < 0)
            return n1;

        return (n1 > 0 ? n1 : 0)
            + (n2 > 0 ? n2 : 0)
            + (n3 > 0 ? n3 : 0);
    }
};

} // namespace netty
