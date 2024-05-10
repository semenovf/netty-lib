////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.05.07 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <pfs/i18n.hpp>
#include <netty/error.hpp>
#include <netty/send_result.hpp>
#include <netty/socket4_addr.hpp>
#include <pfs/ring_buffer.hpp>
#include <pfs/string_view.hpp>
#include <functional>
#include <chrono>
#include <map>
#include <vector>

namespace netty {

template <typename ServerPoller
    , typename ClientPoller
    , typename ListenerSocket
    , typename Socket
    , typename InputEnvelope
    , typename OutputEnvelope>
class service
{
    using output_queue_type = pfs::ring_buffer<std::vector<char>, 64>;

public:
    using input_envelope_type  = InputEnvelope;
    using output_envelope_type = OutputEnvelope;

    ////////////////////////////////////////////////////////////////////////////
    // server
    ////////////////////////////////////////////////////////////////////////////
    class server: protected ServerPoller
    {
        using base_class = ServerPoller;

        struct client_account
        {
            Socket sock;
            bool can_write {true};
            std::vector<char> inb;  // Input buffer
            output_queue_type outq; // Output queue
        };

    public:
        using native_socket_type = typename ServerPoller::native_socket_type;

    private:
        ListenerSocket _listener;
        std::map<native_socket_type, client_account> _clients;

    public:
        mutable std::function<void (error const &)> on_failure = [] (error const &) {};
        mutable std::function<void (std::string const & )> on_error = [] (std::string const & ) {};
        mutable std::function<void (native_socket_type, InputEnvelope const &)> on_message_received
            = [] (native_socket_type, InputEnvelope const &) {};

    private:
        std::function<native_socket_type(native_socket_type, bool &)> accept_proc ()
        {
            return [this] (native_socket_type listener_sock, bool & success) {
                netty::error err;
                auto accepted_sock = _listener.accept_nonblocking(listener_sock, & err);

                if (err) {
                    on_error(tr::f_("accept connection failure: {}", err.what()));
                    success = false;
                    return Socket::kINVALID_SOCKET;
                } else {
                    success = true;
                }

                client_account c;
                c.sock = std::move(accepted_sock);
                auto res = _clients.emplace(c.sock.native(), std::move(c));

                if (res.second) {
                    client_account & c = res.first->second;
                    return c.sock.native();
                }

                on_error(tr::_("socket already exists with the same identifier"));
                return Socket::kINVALID_SOCKET;
            };
        }

    public:
        server (socket4_addr listener_saddr, int backlog = 10)
            : base_class(accept_proc())
            , _listener(std::move(listener_saddr))
        {
            base_class::on_listener_failure = [this] (native_socket_type, error const & err) {
                this->on_failure(err);
            };

            base_class::on_failure = [this] (native_socket_type, error const & err) {
                this->on_failure(err);
            };

            base_class::ready_read = [this] (native_socket_type sock) {
                this->process_input(sock);
            };

            base_class::disconnected = [this] (native_socket_type sock) {
                _clients.erase(sock);
            };

            base_class::can_write = [this] (native_socket_type sock) {
                auto aclient = locate_account(sock);

                if (aclient == nullptr)
                    return;

                aclient->can_write = true;
            };

            base_class::released = [this] (native_socket_type sock) {
                auto aclient = locate_account(sock);

                if (aclient == nullptr)
                    return;

                aclient->sock.disconnect();
            };

            this->add_listener(_listener);
            _listener.listen(backlog);
        }

    public:
        void step (std::chrono::milliseconds timeout = std::chrono::milliseconds{0})
        {
            if (send_outgoing_data() > 0)
                timeout = std::chrono::milliseconds{0};

            this->poll(timeout);
        }

        void enqueue (native_socket_type sock, char const * data, int len)
        {
            auto aclient = locate_account(sock);

            if (aclient == nullptr)
                return;

            aclient.outq.push(std::vector<char>(data, len));
        }

        void enqueue (native_socket_type sock, std::string const & data)
        {
            enqueue(sock, data.data(), data.size());
        }

        void enqueue (native_socket_type sock, pfs::string_view data)
        {
            enqueue(sock, data.data(), data.size());
        }

        void enqueue (native_socket_type sock, std::vector<char> const & data)
        {
            enqueue(sock, data.data(), data.size());
        }

        void enqueue (native_socket_type sock, std::vector<char> && data)
        {
            auto aclient = locate_account(sock);

            if (aclient == nullptr)
                return;

            aclient->outq.push(std::move(data));
        }

    private:
        client_account * locate_account (native_socket_type sock)
        {
            auto pos = _clients.find(sock);

            if (pos == _clients.end()) {
                on_failure(error {
                      make_error_code(pfs::errc::unexpected_error)
                    , tr::f_("client socket not found: {}", sock)
                });

                return nullptr;
            }

            return & pos->second;
        }

        void process_input (native_socket_type sock)
        {
            auto aclient = locate_account(sock);

            if (aclient == nullptr)
                return;

            auto & reader = aclient->sock;
            auto & inb = aclient->inb;

            auto available = reader.available();
            auto offset = inb.size();
            inb.resize(offset + available);
            auto n = reader.recv(inb.data() + offset, available);

            if (n < 0) {
                on_error(tr::f_("receive data failure from: {}, disconnecting (socket={})"
                    , to_string(reader.saddr()), sock));
                reader.disconnect();
                return;
            }

            typename InputEnvelope::deserializer_type in {inb.data(), inb.size()};

            while (in) {
                InputEnvelope env {in};

                // Envelope is corrupted
                if (!env) {
                    on_error(tr::_("bad envelope received, disconnecting"));
                    reader.disconnect();
                    return;
                }

                on_message_received(sock, env);
            }

            auto bytes_processed = std::distance(& *inb.cbegin(), in.begin());

            if (bytes_processed > 0)
                inb.erase(inb.cbegin(), inb.cbegin() + bytes_processed);
        }

        std::size_t send_outgoing_data ()
        {
            std::size_t total_bytes_sent = 0;

            error err;
            bool break_sending = false;

            for (auto & item: _clients) {
                auto * aclient = & item.second;

                if (!aclient->can_write)
                    continue;

                while (!break_sending && !aclient->outq.empty()) {
                    auto & data = aclient->outq.front();
                    auto sendresult = aclient->sock.send(data.data(), data.size(), & err);

                    switch (sendresult.state) {
                        case netty::send_status::good:
                            if (sendresult.n > 0) {
                                if (sendresult.n == data.size()) {
                                    aclient->outq.pop();
                                } else {
                                    data.erase(data.cbegin(), data.cbegin() + sendresult.n);
                                }

                                total_bytes_sent += sendresult.n;
                            }

                            break;

                        case send_status::failure:
                        case send_status::network:
                            base_class::remove(aclient->sock);
                            aclient->sock.disconnect();

                            break_sending = true;
                            on_failure(err);
                            break;

                        case send_status::again:
                        case send_status::overflow:
                            if (aclient->can_write != false) {
                                aclient->can_write = false;
                                this->wait_for_write(aclient->sock);
                            }

                            break_sending = true;
                            break;
                    }
                }
            }

            return total_bytes_sent;
        }
    };

    ////////////////////////////////////////////////////////////////////////////
    // client
    ////////////////////////////////////////////////////////////////////////////
    class client: protected ClientPoller
    {
        using base_class = ClientPoller;
        using native_socket_type = typename ServerPoller::native_socket_type;

    public:
        using output_envelope_type = OutputEnvelope;

    private:
        Socket _sock;
        bool _can_write {false};
        output_queue_type _q;
        std::vector<char> _inb;

    public:
        mutable std::function<void (error const &)> on_failure = [] (error const &) {};
        mutable std::function<void (std::string const & )> on_error = [] (std::string const & ) {};
        mutable std::function<void ()> connected = [] () {};
        mutable std::function<void ()> connection_refused = [] () {};
        mutable std::function<void ()> disconnected = [] () {};
        mutable std::function<void (InputEnvelope const &)> on_message_received
            = [] (InputEnvelope const &) {};

    public:
        client ()
        {
            base_class::on_failure = [this] (native_socket_type, error const & err) {
                this->on_failure(err);
            };

            base_class::connection_refused = [this] (native_socket_type) {
                this->connection_refused();
            };

            base_class::connected = [this] (native_socket_type) {
                base_class::wait_for_write(_sock);
                this->connected();
            };

            base_class::disconnected = [this] (native_socket_type) {
                this->disconnected();
            };

            base_class::ready_read = [this] (native_socket_type sock) {
                this->process_input(sock);
            };

            base_class::can_write = [this] (native_socket_type) {
                _can_write = true;
            };

            base_class::released = [this] (native_socket_type) {
                _sock.disconnect();
                _sock = Socket{};
                _can_write = false;
            };
        }

        ~client () = default;

        /**
         * @throws netty::error {...} See Socket::connect specific exceptions.
         */
        bool connect (netty::socket4_addr listener_saddr, netty::error * perr = nullptr)
        {
            netty::error err;
            auto conn_state = _sock.connect(listener_saddr, & err);

            if (!err)
                this->add(_sock, conn_state, & err);

            if (err) {
                pfs::throw_or(perr, std::move(err));
                return false;
            }

            return true;
        }

        void disconnect (netty::error * perr = nullptr)
        {
            base_class::remove(_sock);
        }

        void enqueue (char const * data, int len)
        {
            _q.push(std::vector<char>(data, len));
        }

        void enqueue (std::string const & data)
        {
            _q.push(std::vector<char>(data.data(), data.size()));
        }

        void enqueue (pfs::string_view data)
        {
            _q.push(std::vector<char>(data.data(), data.size()));
        }

        void enqueue (std::vector<char> const & data)
        {
            _q.push(data);
        }

        void enqueue (std::vector<char> && data)
        {
            _q.push(std::move(data));
        }

        void step (std::chrono::milliseconds timeout = std::chrono::milliseconds{0})
        {
            if (!_q.empty())
                timeout = std::chrono::milliseconds{0};

            this->poll(timeout);
            send_outgoing_data();
        }

    private:
        void process_input (native_socket_type /*sock*/)
        {
            netty::error err;
            auto available = _sock.available();
            auto offset = _inb.size();
            _inb.resize(offset + available);
            auto n = _sock.recv(_inb.data() + offset, available, & err);

            if (n < 0) {
                on_error(tr::f_("receive data failure from: {}, disconnecting (socket={})"
                    , to_string(_sock.saddr()), _sock.native()));
                _sock.disconnect();
                return;
            }

            typename InputEnvelope::deserializer_type in {_inb.data(), _inb.size()};

            while (in) {
                InputEnvelope env {in};

                // Envelope is corrupted
                if (!env) {
                    on_error(tr::_("bad envelope received, disconnecting"));
                    _sock.disconnect();
                    return;
                }

                on_message_received(env);
            }

            auto bytes_processed = std::distance(& *_inb.cbegin(), in.begin());

            if (bytes_processed > 0)
                _inb.erase(_inb.cbegin(), _inb.cbegin() + bytes_processed);
        }

        void send_outgoing_data ()
        {
            error err;
            bool break_sending = false;

            if (!_can_write)
                return;

            while (!break_sending && !_q.empty()) {
                auto & data = _q.front();
                auto sendresult = _sock.send(data.data(), data.size(), & err);

                switch (sendresult.state) {
                    case netty::send_status::good:
                        if (sendresult.n > 0) {
                            if (sendresult.n == data.size()) {
                                _q.pop();
                            } else {
                                data.erase(data.cbegin(), data.cbegin() + sendresult.n);
                            }
                        }

                        break;

                    case send_status::failure:
                    case send_status::network:
                        base_class::remove(_sock);
                        _sock.disconnect();

                        break_sending = true;
                        on_failure(err);
                        break;

                    case send_status::again:
                    case send_status::overflow:
                        if (_can_write != false) {
                            _can_write = false;
                            this->wait_for_write(_sock);
                        }

                        break_sending = true;
                        break;
                }
            }
        }
    };
};

} // namespace netty
