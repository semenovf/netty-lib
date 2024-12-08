////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.02.16 Initial version.
//      2024.04.08 Moved to `utils` namespace.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/netty/error.hpp"
#include "pfs/netty/exports.hpp"
#include "pfs/netty/inet4_addr.hpp"
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace netty {
namespace utils {

struct netlink_attributes
{
    std::string iface_name; // network interface name
    std::uint32_t mtu;
    //bool running; // Interface RFC2863 OPER_UP
    bool up;      // Interface is up
};

// NOTE
// On Linux, it is possible to receive more than one message per interface sequentially
// for `inet4_addr_added` event.
//
class netlink_monitor
{
    class impl;

private:
    std::unique_ptr<impl> _d;

public:
    mutable std::function<void (error const &)> on_failure = [] (error const &) {};
    mutable std::function<void(inet4_addr, std::uint32_t)> inet4_addr_added;
    mutable std::function<void(inet4_addr, std::uint32_t)> inet4_addr_removed;
    // mutable std::function<void(inet6_addr, int)> inet6_addr_added;
    // mutable std::function<void(inet6_addr, int)> inet6_addr_removed;

    // Unused in Windows
    mutable std::function<void(netlink_attributes const &)> attrs_ready;

public:
    NETTY__EXPORT netlink_monitor (error * perr = nullptr);
    NETTY__EXPORT ~netlink_monitor ();
    NETTY__EXPORT int poll (std::chrono::milliseconds millis, error * perr = nullptr);
};

}} // namespace netty::utils
