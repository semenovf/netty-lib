////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.02.16 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/netty/exports.hpp"
#include "pfs/netty/inet4_addr.hpp"
#include "pfs/netty/reader_poller.hpp"
#include "pfs/netty/linux/epoll_poller.hpp"
#include "netlink_socket.hpp"
#include <chrono>
#include <functional>
#include <string>

namespace netty {
namespace linux_os {

struct netlink_attributes
{
    std::string iface_name; // network interface name
    std::uint32_t mtu;
    bool running; // Interface RFC2863 OPER_UP
    bool up;      // Interface is up
};

class netlink_monitor
{
    netlink_socket _nls;
    reader_poller<epoll_poller> _poller;

public:
    mutable std::function<void(error const &)> on_failure;
    mutable std::function<void(netlink_attributes const &)> attrs_ready;
    mutable std::function<void(inet4_addr, std::uint32_t)> inet4_addr_added;
    mutable std::function<void(inet4_addr, std::uint32_t)> inet4_addr_removed;
    //mutable std::function<void(inet6_addr, int)> inet6_addr_added;
    //mutable std::function<void(inet6_addr, int)> inet6_addr_removed;

public:
    NETTY__EXPORT netlink_monitor ();
    NETTY__EXPORT int poll (std::chrono::milliseconds millis, error * perr = nullptr);
};

}} // namespace netty::linux_os
