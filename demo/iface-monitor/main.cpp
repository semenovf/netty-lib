////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2023-2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.02.15 Initial version (netty-lib).
//      2024.04.08 Windows support implemented.
////////////////////////////////////////////////////////////////////////////////
#include "netty/utils/netlink_monitor.hpp"
#include "pfs/filesystem.hpp"
#include "pfs/log.hpp"

using netlink_monitor = netty::utils::netlink_monitor;
using netlink_attributes = netty::utils::netlink_attributes;

int main (int, char *[])
{
    LOGD("", "Start Netlink monitoring");

    netlink_monitor nm;

    nm.on_failure = [] (netty::error const & err) {
        LOGE("", "{}", err.what());
    };

    nm.attrs_ready = [] (netlink_attributes const & attrs) {
        LOGD("", "Link: {} [{}]: mtu={}"
            , attrs.iface_name
            , attrs.up ? "UP" : "DOWN"
            , attrs.mtu);
    };

    nm.inet4_addr_added = [] (netty::inet4_addr addr, std::uint32_t iface_index) {
        LOGD("", "Address added to interface {}: {}", iface_index, to_string(addr));
    };

    nm.inet4_addr_removed = [] (netty::inet4_addr addr, std::uint32_t iface_index) {
        LOGD("", "Address removed from interface {}: {}", iface_index, to_string(addr));
    };

    while (true) {
        nm.poll(std::chrono::seconds{5});
    }

    return EXIT_SUCCESS;
}
