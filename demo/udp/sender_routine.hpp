////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.15 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/log.hpp"
#include "pfs/netty/socket4_addr.hpp"
#include "pfs/netty/posix/udp_sender.hpp"
#include <chrono>
#include <map>
#include <thread>

void run_sender (netty::socket4_addr const & dest_saddr, netty::inet4_addr local_addr)
{
    LOGD(TAG, "Run {} sender to: {}"
        , (netty::is_multicast(dest_saddr.addr)
            ? "MULTICAST"
                : netty::is_broadcast(dest_saddr.addr)
                ? "BROADCAST"
                : "UNICAST")
        , to_string(dest_saddr));

    try {
        netty::posix::udp_sender sender;

        if (netty::is_multicast(dest_saddr.addr)) {
            sender.set_multicast_interface(local_addr);
            LOGD(TAG, "Multicast interface: {}", to_string(local_addr));
        } else if (netty::is_broadcast(dest_saddr.addr)) {
            sender.enable_broadcast(true);
            LOGD(TAG, "Broadcast enabled");
        }

        int counter = 10;

        while (counter--) {
            char helo[] = {'H', 'e', 'l', 'o'};
            auto n = sender.send_to(dest_saddr, helo, sizeof(helo));

            LOGD(TAG, "Send data (counter={}) to: {}, size={}", counter
                , to_string(dest_saddr), n);

            std::this_thread::sleep_for(std::chrono::seconds{1});
        }

        char quit[] = {'Q', 'U', 'I', 'T'};
        sender.send_to(dest_saddr, quit, sizeof(quit));
    } catch (netty::error const & ex) {
        LOGE(TAG, "ERROR: {}", ex.what());
    }
}
