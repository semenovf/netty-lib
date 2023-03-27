////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.02.15 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "pfs/netty/linux/netlink_socket.hpp"
#include "pfs/netty/linux/netlink_monitor.hpp"
#include "pfs/filesystem.hpp"
#include "pfs/fmt.hpp"
#include "pfs/log.hpp"
#include "pfs/string_view.hpp"

static char const * TAG = "netty";

namespace fs = pfs::filesystem;
using netlink_attributes = netty::linux_os::netlink_attributes;
using netlink_socket     = netty::linux_os::netlink_socket;
using netlink_monitor    = netty::linux_os::netlink_monitor;

static struct program_context {
    std::string program;
} __pctx;

static void print_usage ()
{
    fmt::print(stdout, "Usage\n{}\n", __pctx.program);
//     fmt::print(stdout, "\t--discovery-saddr=ADDR:PORT\n");
//     fmt::print(stdout, "\t--target-saddr=ADDR:PORT...\n");
}

int main (int argc, char * argv[])
{
    __pctx.program = fs::utf8_encode(fs::utf8_decode(argv[0]).filename());

    //if (argc == 1) {
        // print_usage();
        // return EXIT_SUCCESS;
    //}

    for (int i = 1; i < argc; i++) {
        if (string_view{"-h"} == argv[i] || string_view{"--help"} == argv[i]) {
            print_usage();
            return EXIT_SUCCESS;
        } else {
            ;
        }
    }

    fmt::print("Start Netlink monitoring\n");

    netlink_monitor nm;

    nm.attrs_ready = [] (netlink_attributes const & attrs) {
        fmt::print("{} [{}] [{}]: mtu={}\n", attrs.iface_name
            , attrs.running ? "RUNNING" : "NOT RUNNING"
            , attrs.up ? "UP" : "DOWN"
            , attrs.mtu);
    };

    nm.inet4_addr_added = [] (netty::inet4_addr addr, std::uint32_t iface_index) {
        LOGD("", "Address added to interface {}: {}", iface_index, to_string(addr));
    };

    nm.inet4_addr_removed = [] (netty::inet4_addr addr, std::uint32_t iface_index) {
        LOGD("", "Address removed from interface {}: {}", iface_index, to_string(addr));
    };

    while(true)
        nm.poll(std::chrono::seconds{1});

    return EXIT_SUCCESS;
}
