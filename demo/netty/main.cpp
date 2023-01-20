////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.01 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "pfs/fmt.hpp"
#include "pfs/integer.hpp"
#include "pfs/log.hpp"
#include "pfs/string_view.hpp"
#include "pfs/netty/startup.hpp"
#include <algorithm>
#include <map>
#include <string>
#include <vector>
#include <cstdlib>

static char const * TAG = "NETTY";

#include "types.hpp"
#include "client_routine.hpp"
#include "server_routine.hpp"

using string_view = pfs::string_view;

static struct program_context {
    std::string program;
    std::vector<std::string> poller_variants;
    std::string poller_variants_string;
} __pctx;

static void print_usage ()
{
    fmt::print(stdout, "Usage\n\t{} --poller={} --tcp|--udp [--server]\n"
        , __pctx.program, __pctx.poller_variants_string);
    fmt::print(stdout, "\t\t--addr=ip4_addr [--port=port]\n");
    fmt::print(stdout, "\nRun TCP server\n\t{} --tcp --server --addr=127.0.0.1\n", __pctx.program);
    fmt::print(stdout, "\nSend echo packets to TCP server\n\t{} --tcp --addr=127.0.0.1\n", __pctx.program);
    fmt::print(stdout, "\nRun UDP server\n\t{} --udp --server --addr=127.0.0.1\n", __pctx.program);
    fmt::print(stdout, "\nSend echo packets to UDP server\n\t{} --udp --addr=127.0.0.1\n", __pctx.program);
    fmt::print(stdout, "\n\nNotes:\n");
    fmt::print(stdout, "\t'select', 'poll' and 'epoll' pollers on Linux are compatible,\n");
    fmt::print(stdout, "\ti.e. server and client sides can be different types.\n");
}

template <typename Iter>
static std::string concat (Iter first, Iter last, std::string const & separator)
{
    std::string result;

    if (first != last) {
        result += *first;
        ++first;
    }

    for (; first != last; ++first) {
        result += separator;
        result += *first;
    }

    return result;
}

int main (int argc, char * argv[])
{
    netty::startup_guard netty_startup;

    bool is_server = false;
    bool is_tcp = false;
    bool is_udp = false;
    std::string addr_value;
    std::string port_value;
    std::string poller_value;

    __pctx.program = argv[0];

#if NETTY__POLL_ENABLED
    __pctx.poller_variants.push_back("poll");
#endif

#if NETTY__EPOLL_ENABLED
    __pctx.poller_variants.push_back("select");
#endif

#if NETTY__SELECT_ENABLED
    __pctx.poller_variants.push_back("epoll");
#endif

#if NETTY__UDT_ENABLED
    __pctx.poller_variants.push_back("udt");
#endif

    __pctx.poller_variants_string = concat(
          __pctx.poller_variants.begin()
        , __pctx.poller_variants.end()
        , std::string{"|"});

    if (argc == 1) {
        print_usage();
        return EXIT_SUCCESS;
    }

    for (int i = 1; i < argc; i++) {
        if (string_view{"-h"} == argv[i] || string_view{"--help"} == argv[i]) {
            print_usage();
            return EXIT_SUCCESS;
        } else if (string_view{"--server"} == argv[i]) {
            is_server = true;
        } else if (string_view{"--udp"} == argv[i]) {
            if (is_tcp) {
                LOGE(TAG, "Only one of --udp or --tcp must be specified");
                return EXIT_FAILURE;
            }
            is_udp = true;
        } else if (string_view{"--tcp"} == argv[i]) {
            if (is_udp) {
                LOGE(TAG, "Only one of --udp or --tcp must be specified");
                return EXIT_FAILURE;
            }

            is_tcp = true;
        } else if (string_view{argv[i]}.starts_with("--poller=")) {
            poller_value = std::string{argv[i] + 9};
        } else if (string_view{argv[i]}.starts_with("--addr=")) {
            addr_value = std::string{argv[i] + 7};
        } else if (string_view{argv[i]}.starts_with("--port=")) {
            port_value = std::string{argv[i] + 7};
        } else {
            auto arglen = std::strlen(argv[i]);

            if (arglen > 0 && argv[i][0] == '-') {
                LOGE(TAG, "Bad option: {}", argv[i]);
                return EXIT_FAILURE;
            }
        }
    }

    if (poller_value.empty()) {
        LOGE(TAG, "No poller specified");
        return EXIT_FAILURE;
    }

    auto is_valid_poller = (std::find(__pctx.poller_variants.begin()
        , __pctx.poller_variants.end(), poller_value)
        != __pctx.poller_variants.end());

    if (!is_valid_poller) {
        LOGE(TAG, "Invalid poller");
        return EXIT_FAILURE;
    }

    if (addr_value.empty()) {
        LOGE(TAG, "No address specified");
        return EXIT_FAILURE;
    }

    auto res = netty::inet4_addr::parse(addr_value);

    if (!res.first) {
        LOGE(TAG, "Bad address");
        return EXIT_FAILURE;
    }

    auto addr = res.second;
    std::uint16_t port = 42942;

    if (!port_value.empty()) {
        std::error_code ec;
        port = pfs::to_integer(port_value.begin(), port_value.end()
            , std::uint16_t{1024}, std::uint16_t{65535}, ec);

        if (ec) {
            LOGE(TAG, "Bad port");
            return EXIT_FAILURE;
        }
    }

    if (is_server) {
        if (poller_value == "udt") {
            start_server<netty::server_udt_poller_type, udt_server_type, udt_socket_type>(netty::socket4_addr{addr, port});
        } else if (is_tcp) {
            if (poller_value == "select")
                start_server<netty::server_select_poller_type, tcp_server_type, tcp_socket_type>(netty::socket4_addr{addr, port});
            else if (poller_value == "poll")
                start_server<netty::server_poll_poller_type, tcp_server_type, tcp_socket_type>(netty::socket4_addr{addr, port});
            else if (poller_value == "epoll")
                start_server<netty::server_epoll_poller_type, tcp_server_type, tcp_socket_type>(netty::socket4_addr{addr, port});
        } else {
            LOGE(TAG, "UDP server not implemented yet");
        }
    } else {
        if (poller_value == "udt") {
            start_client<netty::client_udt_poller_type, udt_socket_type>(netty::socket4_addr{addr, port});
        } else if (is_tcp) {
            if (poller_value == "select")
                start_client<netty::client_select_poller_type, tcp_socket_type>(netty::socket4_addr{addr, port});
            else if (poller_value == "poll")
                start_client<netty::client_poll_poller_type, tcp_socket_type>(netty::socket4_addr{addr, port});
            else if (poller_value == "epoll")
                start_client<netty::client_epoll_poller_type, tcp_socket_type>(netty::socket4_addr{addr, port});
        } else if (is_udp) {
            LOGE(TAG, "UDP client not implemented yet");
        }
    }

    return EXIT_SUCCESS;
}
