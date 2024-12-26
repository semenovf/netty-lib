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
#include "exports.hpp"
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
    using backend_type = Backend;
    using socket_id = typename Backend::socket_id;

private:
    connecting_poller<Backend> _connecting_poller;
    reader_poller<Backend> _reader_poller;
    writer_poller<Backend> _writer_poller;
    std::vector<socket_id> _addable_connecting_sockets;
    std::vector<socket_id> _addable_readers;
    std::vector<socket_id> _removable_connecting_sockets;
    std::vector<socket_id> _removable_readers;
    std::vector<socket_id> _removable_writers;
    std::set<socket_id>    _removable;

    // Reader and writer pollers backends are shared or not
    bool _is_pollers_shared {false};

public:
    mutable std::function<void(socket_id, error const &)> on_failure = [] (socket_id, error const &) {};
    mutable std::function<void(socket_id, connection_refused_reason)> connection_refused
        = [] (socket_id, connection_refused_reason) {};
    mutable std::function<void(socket_id)> connected = [] (socket_id) {};

    /**
     * Socket has been disconnected by the peer. No disconnection required.
     */
    mutable std::function<void(socket_id)> disconnected = [] (socket_id) {};

    mutable std::function<void(socket_id)> ready_read = [] (socket_id) {};
    mutable std::function<void(socket_id)> can_write = [] (socket_id) {};

    /**
     * Socket has been removed from client poller. It can be disconnected and/or released safely.
     */
    mutable std::function<void(socket_id)> removed = [] (socket_id) {};

private:
    void init_callbacks ()
    {
        _connecting_poller.on_failure = [this] (socket_id sock, error const & err) {
            // The socket must later be removed from monitoring
            _removable_connecting_sockets.push_back(sock);
            _removable.insert(sock);
            on_failure(sock, err);
        };

        _reader_poller.on_failure = [this] (socket_id sock, error const & err) {
            // The socket must later be removed from monitoring
            _removable_readers.push_back(sock);
            _removable.insert(sock);
            on_failure(sock, err);
        };

        _writer_poller.on_failure = [this] (socket_id sock, error const & err) {
            // The socket must later be removed from monitoring
            _removable_writers.push_back(sock);
            _removable.insert(sock);
            on_failure(sock, err);
        };

        _connecting_poller.connection_refused = [this] (socket_id sock, connection_refused_reason reason) {
            // The socket must later be removed from monitoring
            _removable_connecting_sockets.push_back(sock);
            _removable.insert(sock);
            connection_refused(sock, reason);
        };

        _connecting_poller.connected = [this] (socket_id sock) {
            // The socket must later be removed from monitoring
            if (!_is_pollers_shared) {
                _removable_connecting_sockets.push_back(sock);
                _reader_poller.add(sock); // safe to add to poller
            }

            connected(sock);
        };

        _reader_poller.disconnected = [this] (socket_id sock) {
            // The socket must later be removed from monitoring
            _removable_readers.push_back(sock);
            _removable.insert(sock);
            disconnected(sock);
        };

        _reader_poller.ready_read = [this] (socket_id sock) {
            ready_read(sock);
        };

        _writer_poller.can_write = [this] (socket_id sock) {
            // If writer poller is shared, so no need to remove socket from it
            if (!_is_pollers_shared)
                _removable_writers.push_back(sock);

            can_write(sock);
        };
    }

    client_poller (std::shared_ptr<Backend> shared_poller_backend)
        : _connecting_poller(shared_poller_backend)
        , _reader_poller(shared_poller_backend)
        , _writer_poller(shared_poller_backend)
        , _is_pollers_shared(shared_poller_backend != nullptr)
    {
        init_callbacks();
    }

public:
    NETTY__EXPORT client_poller (); // Backend specific
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
            _addable_connecting_sockets.push_back(sock.id());
        } else if (state == conn_status::connected) {
            _addable_readers.push_back(sock.id());
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
     * Remove socket from connecting and regular pollers.
     */
    template <typename Socket>
    void remove (Socket const & sock, error * /*perr*/ = nullptr)
    {
        _removable_connecting_sockets.push_back(sock.id());
        _removable_readers.push_back(sock.id());
        _removable_writers.push_back(sock.id());
        _removable.insert(sock.id());

        LOG_TRACE_3("Client socket ({}) removed from `client_poller`", to_string(sock.saddr()));
    }

    /**
     * Add socket to writer poller to waiting for socket to be able to write.
     * It will be removed automatically from writer poller.
     */
    template <typename Socket>
    void wait_for_write (Socket const & sock, error * perr = nullptr)
    {
        _writer_poller.wait_for_write(sock.id(), perr);
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
                _reader_poller.add(sock);
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
