////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.18 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/netty/poller_types.hpp"
#include "pfs/netty/reader_poller.hpp"
#include "pfs/netty/qt5/udp_receiver.hpp"
#include "pfs/netty/qt5/udp_sender.hpp"
#include <map>
#include <utility>
#include <vector>

#if defined(_MSC_VER) || defined(NETTY__SELECT_ENABLED)
#   include "pfs/netty/posix/select_poller.hpp"
#endif

#if defined(ANDROID) || defined(NETTY__POLL_ENABLED)
#   include "pfs/netty/posix/poll_poller.hpp"
#endif

#if defined(NETTY__EPOLL_ENABLED)
#   include "pfs/netty/linux/epoll_poller.hpp"
#endif

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
#if _MSC_VER
    using poller_type = netty::reader_poller<netty::posix::select_poller>;
#elif ANDROID
    using poller_type = netty::reader_poller<netty::posix::poll_poller>;
#elif NETTY__EPOLL_ENABLED
    using poller_type = netty::reader_poller<netty::linux_os::epoll_poller>;
#elif NETTY__POLL_ENABLED
    using poller_type = netty::reader_poller<netty::posix::poll_poller>;
#elif NETTY__SELECT_ENABLED
    using poller_type = netty::reader_poller<netty::posix::select_poller>;
#else
#   error "No poller for discovery engine found"
#endif

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

    NETTY__EXPORT int send (socket4_addr dest_saddr, char const * data
        , std::size_t len, netty::error * perr);
};

}}} // namespace netty::p2p::qt5
