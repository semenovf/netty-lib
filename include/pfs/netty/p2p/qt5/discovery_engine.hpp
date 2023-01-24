////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.18 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/netty/regular_poller.hpp"
#include "pfs/netty/posix/poll_poller.hpp"
#include "pfs/netty/qt5/udp_receiver.hpp"
#include "pfs/netty/qt5/udp_sender.hpp"
#include <map>
#include <utility>
#include <vector>

namespace netty {
namespace p2p {
namespace qt5 {

// TODO Need complete implementation
class discovery_engine
{
public:
    using receiver_type = discovery_engine;
    using sender_type   = discovery_engine;

private:
    using poller_type = netty::regular_poller<netty::qt5::poll_poller>;

private:
    poller_type _poller;
    std::map<poller_type::native_socket_type, netty::qt5::udp_receiver> _listeners;
    std::vector<std::pair<socket4_addr, netty::qt5::udp_sender>> _targets;

public:
    std::function<void (socket4_addr /*saddr*/, std::vector<char> /*data*/)> data_ready;

public:
    NETTY__EXPORT discovery_engine ();
    NETTY__EXPORT ~discovery_engine ();

    /**
     * Add listener.
     *
     * @param src_saddr Listener address (unicast, multicast or broadcast).
     * @param local_addr Local address for multicast or broadcast.
     */
    NETTY__EXPORT void add_listener (socket4_addr src_saddr
        , inet4_addr local_addr = inet4_addr::any_addr_value);

    /**
     * Add target.
     *
     * @param dest_saddr Target address (unicast, multicast or broadcast).
     * @param local_addr Multicast interface.
     */
    NETTY__EXPORT void add_target (socket4_addr dest_saddr
        , inet4_addr local_addr = inet4_addr::any_addr_value);

    NETTY__EXPORT std::pair<int,int> poll (std::chrono::milliseconds timeout);

    NETTY__EXPORT std::streamsize send (socket4_addr dest_saddr, char const * data
        , std::size_t size, netty::error * perr);
};

}}} // namespace netty::p2p::qt5
