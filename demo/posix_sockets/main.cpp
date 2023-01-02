////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.01 Initial version.
////////////////////////////////////////////////////////////////////////////////
#define PFS__LOG_LEVEL 2
#include "pfs/fmt.hpp"
#include "pfs/integer.hpp"
#include "pfs/log.hpp"
#include "pfs/string_view.hpp"
#include "pfs/netty/inet4_addr.hpp"
#include "pfs/netty/poller.hpp"
#include "pfs/netty/socket4_addr.hpp"
#include "pfs/netty/posix/tcp_server.hpp"
#include "pfs/netty/posix/tcp_socket.hpp"
#include <map>
#include <cstdlib>

#if NETTY__EPOLL_ENABLED
#   include "pfs/netty/linux/epoll_poller.hpp"
using poller_type = netty::poller<netty::linux_ns::epoll_poller>;
#endif


static char const * TAG = "POSIX_SOCKETS";
using string_view = pfs::string_view;

void print_usage (char const * program)
{
    fmt::print(stdout, "Usage\n\t{} --tcp|--udp [--server] --addr=ip4_addr [--port=port]\n", program);
    fmt::print(stdout, "\nRun TCP server\n\t{} --tcp --server --addr=127.0.0.1\n", program);
    fmt::print(stdout, "\nSend echo packets to TCP server\n\t{} --tcp --addr=127.0.0.1\n", program);
    fmt::print(stdout, "\nRun UDP server\n\t{} --udp --server --addr=127.0.0.1\n", program);
    fmt::print(stdout, "\nSend echo packets to UDP server\n\t{} --udp --addr=127.0.0.1\n", program);
}

void start_tcp_server (netty::socket4_addr const & saddr)
{
    LOGD(TAG, "Starting TCP server on: {}", to_string(saddr));

    try {
        netty::posix::tcp_server tcp_server {saddr, 10};
        poller_type server_poller;
        poller_type client_poller;
        std::chrono::milliseconds poller_timeout {1000};
        std::chrono::milliseconds poller_immediate {0};

        std::map<netty::posix::tcp_socket::native_type, netty::posix::tcp_socket> _clients;

        server_poller.on_error = [] (netty::posix::tcp_server::native_type) {
            LOGE(TAG, "Error on server");
        };

        server_poller.ready_read = [& tcp_server, & client_poller, & _clients] (
            netty::posix::tcp_server::native_type sock) {

            int n = 0;

            do {
                auto client = tcp_server.accept();

                if (!client)
                    break;

                _clients[sock] = std::move(client);
                client_poller.add(sock);
                n++;
            } while (true);

            LOGD(TAG, "Client(s) accepted: {}", n);
        };

        client_poller.on_error = [] (netty::posix::tcp_socket::native_type) {
            LOGE(TAG, "Error on client");
        };

        client_poller.disconnected = [& client_poller, & _clients] (
            netty::posix::tcp_socket::native_type sock) {

            LOGD(TAG, "Client disconnected");
            client_poller.remove(sock);
            _clients.erase(sock);
        };

        client_poller.ready_read = [] (netty::posix::tcp_socket::native_type) {
            LOGD(TAG, "Client ready_read");
        };

        client_poller.can_write = [] (netty::posix::tcp_socket::native_type) {
            LOGD(TAG, "Client can_write");
        };

        client_poller.unsupported_event = [] (netty::posix::tcp_socket::native_type, int revents) {
            LOGD(TAG, "Has unsupported event(s): {}", revents);
        };

        server_poller.add(tcp_server.native());

        while (true) {
            client_poller.poll(poller_immediate);
            server_poller.poll(poller_timeout);
        }
    } catch (netty::error const & ex) {
        LOGE(TAG, "ERROR: {}", ex.what());
    }
}

void start_tcp_client (netty::socket4_addr const & saddr)
{
    LOGD(TAG, "Starting TCP client");

    try {
        bool finish = false;
        netty::posix::tcp_socket tcp_socket;
        poller_type connecting_poller;
        poller_type client_poller;
        std::chrono::milliseconds poller_timeout {1000};
        std::chrono::milliseconds poller_immediate {0};

        connecting_poller.can_write = [& tcp_socket, & connecting_poller, & client_poller] (
            netty::posix::tcp_server::native_type sock) {

            if (tcp_socket.ensure_connected()) {
                LOGD(TAG, "Client connected");
                connecting_poller.remove(sock);
                client_poller.add(sock);
            }
        };

        client_poller.on_error = [] (netty::posix::tcp_server::native_type) {
            LOGE(TAG, "Error on client");
        };

        client_poller.disconnected = [& finish] (netty::posix::tcp_server::native_type) {
            LOGD(TAG, "Client disconnected");
            finish = true;
        };

        client_poller.ready_read = [] (netty::posix::tcp_server::native_type) {
            LOGD(TAG, "Client ready_read");
        };

        client_poller.can_write = [] (netty::posix::tcp_server::native_type) {
            ;
        };

        connecting_poller.add(tcp_socket.native());

        LOGD(TAG, "Socket connected (before connecting): {}", tcp_socket.connected());
        LOGD(TAG, "Connecting server: {}", to_string(saddr));

        tcp_socket.connect(saddr);

        while (!finish) {
            if (!connecting_poller.empty())
                connecting_poller.poll(poller_immediate);
            client_poller.poll(poller_timeout);
        }
    } catch (netty::error const & ex) {
        LOGE(TAG, "ERROR: {}", ex.what());
    }
}

int main (int argc, char * argv[])
{
    bool is_server = false;
    bool is_tcp = false;
    bool is_udp = false;
    std::string addr_str;
    std::string port_str;

    if (argc == 1) {
        print_usage(argv[0]);
        return EXIT_SUCCESS;
    }

    for (int i = 1; i < argc; i++) {
        if (std::string{"-h"} == argv[i] || std::string{"--help"} == argv[i]) {
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        } else if (std::string{"--server"} == argv[i]) {
            is_server = true;
        } else if (std::string{"--udp"} == argv[i]) {
            if (is_tcp) {
                LOGE(TAG, "Only one of --udp or --tcp must be specified");
                return EXIT_FAILURE;
            }
            is_udp = true;
        } else if (std::string{"--tcp"} == argv[i]) {
            if (is_udp) {
                LOGE(TAG, "Only one of --udp or --tcp must be specified");
                return EXIT_FAILURE;
            }

            is_tcp = true;
        } else if (string_view{argv[i]}.starts_with("--addr=")) {
            addr_str = std::string{argv[i] + 7};
        } else if (string_view{argv[i]}.starts_with("--port=")) {
            port_str = std::string{argv[i] + 7};
        } else {
            auto arglen = std::strlen(argv[i]);

            if (arglen > 0 && argv[i][0] == '-') {
                LOGE(TAG, "Bad option");
                return EXIT_FAILURE;
            }
        }
    }

    if (addr_str.empty()) {
        LOGE(TAG, "No address specified");
        return EXIT_FAILURE;
    }

    auto res = netty::inet4_addr::parse(addr_str);

    if (!res.first) {
        LOGE(TAG, "Bad address");
        return EXIT_FAILURE;
    }

    auto addr = res.second;
    std::uint16_t port = 42942;

    if (!port_str.empty()) {
        std::error_code ec;
        port = pfs::to_integer(port_str.begin(), port_str.end()
            , std::uint16_t{1024}, std::uint16_t{65535}, ec);

        if (ec) {
            LOGE(TAG, "Bad port");
            return EXIT_FAILURE;
        }
    }

    if (is_server) {
        if (is_tcp) {
            start_tcp_server(netty::socket4_addr{addr, port});
        } else {
            LOGE(TAG, "UDP server not implemented yet");
        }
    } else {
        if (is_tcp) {
            start_tcp_client(netty::socket4_addr{addr, port});
        } else if (is_udp) {
            LOGE(TAG, "UDP client not implemented yet");
        }
    }

    return EXIT_SUCCESS;
}
