////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.03.21 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "conn_status.hpp"
#include "error.hpp"
#include "socket4_addr.hpp"
#include "send_result.hpp"
#include "tag.hpp"
#include "pfs/i18n.hpp"
#include "pfs/log.hpp"
#include "pfs/memory.hpp"
#include "pfs/null_mutex.hpp"
#include "startup.hpp"
#include "tag.hpp"
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

namespace netty {

/**
 * @brief Client socket engine.
 *
 * @tparam Socket Socket type (netty::posix::tcp_socket, etc).
 * @tparam ClientPoller Platform specific client poller (netty::client_poller_type, etc).
 *
 * @see <a href="https://en.cppreference.com/w/cpp/named_req/BasicLockable">C++ named requirements: BasicLockable</a>
 */
template <typename Socket, typename ClientPoller, typename Protocol, typename BasicLockable>
class client_socket_engine
{
private:
    using socket_type   = Socket;
    using poller_type   = ClientPoller;
    using protocol_type = Protocol;

public:
    enum class loop_result
    {
          success
        , disconnected
        , connection_refused
        , timedout
    };

    struct options
    {
        // Maximum chunk size to send in one loop iteration.
        int max_chunk_size = 1024;

        std::chrono::milliseconds initial_poller_timeout {0};
        std::chrono::milliseconds poller_timeout_increment {0};
        std::chrono::milliseconds max_poller_timeout {10};

    } _opts;

private:
    socket_type   _socket;
    protocol_type _protocol;
    std::unique_ptr<poller_type> _poller;

    bool _can_write {false};

    BasicLockable _omtx;

    // Output raw data to send
    std::vector<char> _obuf;

    // Input buffer to accumulate raw data
    std::vector<char> _ibuf;

    std::chrono::milliseconds _current_poller_timeout {0};
    loop_result _loop_result = loop_result::success;

    bool _connected {false};

public: // Callbacks
    mutable std::function<void (std::string const &)> on_error
        = [] (std::string const &) {};

    mutable std::function<void (client_socket_engine &)> connected
        = [] (client_socket_engine &) {};

    mutable std::function<void (client_socket_engine &)> disconnected
        = [] (client_socket_engine &) {};

    mutable std::function<void (client_socket_engine &)> connection_refused
        = [] (client_socket_engine &) {};

public:
    client_socket_engine ()
        : client_socket_engine(default_options())
    {}

    /**
     * Initializes underlying APIs and constructs engine instance.
     *
     * @param uuid Unique identifier for this instance.
     */
    client_socket_engine (options const & opts)
    {
        auto bad = false;
        std::string invalid_argument_desc;
        _opts = opts;

        on_error = [] (std::string const & s) {
            LOGE(TAG, "ERROR: {}", s);
        };

        do {
            auto max_chunk_size_max_value = (std::numeric_limits<std::int16_t>::max)();
            bad = _opts.max_chunk_size <= 0
                && _opts.max_chunk_size > max_chunk_size_max_value;

            if (bad) {
                invalid_argument_desc = tr::f_("maximum chunk size must be a"
                    " positive integer and less than {}", max_chunk_size_max_value);
                break;
            }
        } while (false);

        if (bad) {
            throw error {
                  errc::invalid_argument
                , invalid_argument_desc
            };
        }

        _current_poller_timeout = _opts.initial_poller_timeout;

        // Call befor any network operations
        startup();
    }

    ~client_socket_engine ()
    {
        _poller.reset();
        cleanup();
    }

    client_socket_engine (client_socket_engine const &) = delete;
    client_socket_engine & operator = (client_socket_engine const &) = delete;

    client_socket_engine (client_socket_engine &&) = delete;
    client_socket_engine & operator = (client_socket_engine &&) = delete;

    bool connect (netty::socket4_addr const & server_saddr
        , std::chrono::milliseconds timeout = std::chrono::milliseconds{0})
    {
        typename poller_type::callbacks callbacks;

        callbacks.on_error = [this] (typename poller_type::native_socket_type
            , std::string const & text) {
            this->on_error(text);
        };

        callbacks.connection_refused = [this] (typename poller_type::native_socket_type sock) {
            _loop_result = loop_result::connection_refused;
            _connected = false;
            this->connection_refused(*this);
        };

        callbacks.connected = [this] (typename poller_type::native_socket_type sock) {
            _connected = true;
            this->connected(*this);
        };

        callbacks.disconnected = [this] (typename poller_type::native_socket_type sock) {
            _loop_result = loop_result::disconnected;
            _connected = false;
            this->disconnected(*this);
        };

        callbacks.ready_read = [this] (typename poller_type::native_socket_type sock) {
            process_input(sock);
        };

        callbacks.can_write = [this] (typename poller_type::native_socket_type sock) {
            _can_write = true;
        };

        _poller = pfs::make_unique<poller_type>(std::move(callbacks));

        auto conn_state = _socket.connect(server_saddr);

        if (conn_state == conn_status::failure)
            return false;

        if (conn_state == conn_status::connected)
            return true;

        _poller->add(_socket, conn_state);
        _poller->poll_connected(timeout);
        return _connected;
    }

public:
    /**
     * @return the opposite value for what `finish` predicate return.
     */
    loop_result loop ()
    {
        _loop_result = loop_result::success;

        send_outgoing_data();
        auto n = _poller->poll(_current_poller_timeout);

        if (n == 0) {
            _current_poller_timeout += _opts.poller_timeout_increment;

            if (_current_poller_timeout > _opts.max_poller_timeout)
                _current_poller_timeout = _opts.max_poller_timeout;
        } else {
            _current_poller_timeout = _opts.initial_poller_timeout;
        }

        return _loop_result;
    }

//     loop_result wait (std::chrono::milliseconds poller_timeout)
//     {
//         _loop_result = loop_result::success;
//         auto n = _poller->poll(poller_timeout);
//
//         if (_loop_result != loop_result::success)
//             return _loop_result;
//
//         return n == 0 ? loop_result::timedout : _loop_result;
//     }

    /**
     */
    loop_result recv (std::chrono::milliseconds timeout)
    {
        _loop_result = loop_result::success;
        auto n = _poller->poll_read(timeout);

        LOGD(TAG, "Recv: read poller return: {}", n);

        if (_loop_result != loop_result::success)
            return _loop_result;

        return n == 0 ? loop_result::timedout : _loop_result;
    }

    template <typename Packet>
    send_result send (Packet const & p, std::chrono::milliseconds timeout)
    {
        send_async<Packet>(p);
        auto res = send_outgoing_data();
        return res;
    }

    template <typename Packet>
    std::size_t send_async (Packet const & p)
    {
        auto bytes = _protocol.serialize(p);
        enqueue(bytes.data(), bytes.size());
        return bytes.size();
    }

public: // static
    static options default_options () noexcept
    {
        return options{};
    }

private:
    /**
     * Enqueues raw data into internal buffer.
     *
     * @param data Data to send.
     * @param len  Data length.
     */
    void enqueue (char const * data, int len)
    {
        std::unique_lock<BasicLockable> lock(_omtx);

        auto offset = _obuf.size();
        _obuf.resize(offset + len);
        std::memcpy(_obuf.data() + offset, data, len);
    }

    void process_input (typename poller_type::native_socket_type sock)
    {
        if (_socket.native() != sock) {
            on_error(tr::f_("alien socket requested input process, ignored: {}", sock));
            return;
        }

        auto available = _socket.available();
        auto offset = _ibuf.size();
        _ibuf.resize(offset + available);

        auto n = _socket.recv(_ibuf.data() + offset, available);

        if (n < 0) {
            throw error {
                  errc::unexpected_error
                , tr::f_("Receive data failure from: {}", to_string(_socket.saddr()))
            };
        }

        _ibuf.resize(offset + n);

        while (_protocol.has_complete_packet(_ibuf.data(), _ibuf.size())) {
            auto result = _protocol.exec(_ibuf.data(), _ibuf.size());

            if (result.first) {
                auto bytes_processed = result.second;

                if (bytes_processed == _ibuf.size())
                    _ibuf.clear();
                else
                    _ibuf.erase(_ibuf.begin(), _ibuf.begin() + bytes_processed);
            } else {
                if (result.second == (std::numeric_limits<std::size_t>::max)()) {
                    throw error {
                          errc::unexpected_error
                        , tr::f_("Receive bad packet from: {}", to_string(_socket.saddr()))
                    };
                }
            }
        }
    }

    netty::send_result send_outgoing_data ()
    {
        std::unique_lock<BasicLockable> lock(_omtx);

        std::size_t total_bytes_sent = 0;

        error err;
        bool break_sending = false;
        netty::send_status sendstate = netty::send_status::good;

        while (!break_sending && !_obuf.empty()) {
            int nbytes_to_send = _opts.max_chunk_size;

            if (nbytes_to_send > _obuf.size())
                nbytes_to_send = static_cast<int>(_obuf.size());

            auto sendresult = _socket.send(_obuf.data(), nbytes_to_send, & err);
            sendstate = sendresult.state;

            switch (sendstate) {
                case netty::send_status::failure:
                    on_error(tr::f_("send failure to {}: {}"
                        , to_string(_socket.saddr()), err.what()));
                    break_sending = true;
                    break;

                case netty::send_status::network:
                    on_error(tr::f_("send failure to {}: network failure: {}"
                        , to_string(_socket.saddr()), err.what()));
                    break_sending = true;
                    break;

                case netty::send_status::again:
                case netty::send_status::overflow:
                    if (_can_write != false) {
                        _can_write = false;
                        _poller->wait_for_write(_socket);
                    }

                    break_sending = true;
                    break;

                case netty::send_status::good:
                    if (sendresult.n > 0) {
                        total_bytes_sent += sendresult.n;
                        _obuf.erase(_obuf.begin(), _obuf.begin() + sendresult.n);
                    }

                    break;
            }
        }

        return send_result{sendstate, total_bytes_sent};
    }
};

template <typename Socket, typename ClientPoller, typename Protocol>
using client_socket_engine_st = client_socket_engine<Socket, ClientPoller, Protocol, pfs::null_mutex>;

template <typename Socket, typename ClientPoller, typename Protocol>
using client_socket_engine_mt = client_socket_engine<Socket, ClientPoller, Protocol, std::mutex>;

} // namespace netty
