////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.17 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/netty/reader_poller.hpp"
#include "pfs/netty/posix/poll_poller.hpp"
#include "pfs/netty/posix/udp_receiver.hpp"
#include "pfs/netty/posix/udp_sender.hpp"
#include <map>
#include <utility>
#include <vector>

namespace netty {
namespace p2p {
namespace posix {

class discovery_engine
{
public:
    using receiver_type = discovery_engine;
    using sender_type   = discovery_engine;

private:
    using poller_type = netty::reader_poller<netty::posix::poll_poller>;

private:
    poller_type _poller;
    std::map<poller_type::native_socket_type, netty::posix::udp_receiver> _receivers;
    std::vector<std::pair<socket4_addr, netty::posix::udp_sender>> _targets;

public:
    std::function<void (socket4_addr /*saddr*/, std::vector<char> /*data*/)> data_ready;

public:
    NETTY__EXPORT discovery_engine ();
    NETTY__EXPORT ~discovery_engine ();

    /**
     * Add receiver.
     *
     * @param src_saddr Receiver address (unicast, multicast or broadcast).
     * @param local_addr Local address for multicast or broadcast.
     */
    NETTY__EXPORT void add_receiver (socket4_addr src_saddr
        , inet4_addr local_addr = inet4_addr::any_addr_value);

    /**
     * Check if any receivers added to discovery engine.
     */
    NETTY__EXPORT bool has_receivers () const noexcept;

    /**
     * Add target.
     *
     * @param dest_saddr Target address (unicast, multicast or broadcast).
     * @param local_addr Multicast interface.
     */
    NETTY__EXPORT void add_target (socket4_addr dest_saddr
        , inet4_addr local_addr = inet4_addr::any_addr_value);

    /**
     * Check if any targets added to discovery engine.
     */
    NETTY__EXPORT bool has_targets () const noexcept;

    NETTY__EXPORT int poll (std::chrono::milliseconds millis
        , error * perr = nullptr);

    NETTY__EXPORT send_result send (socket4_addr dest_saddr, char const * data
        , std::size_t size, error * perr);
};

}}} // namespace netty::p2p::posix