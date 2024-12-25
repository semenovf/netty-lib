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
#include <chrono>
#include <map>
#include <thread>

using namespace std::chrono_literals;

template <typename Sender>
void run_sender (netty::socket4_addr const & destSaddr, netty::inet4_addr localAddr
    , std::chrono::milliseconds interval, std::uint32_t maxCount, bool quitOnlyPacket)
{
    LOGD(TAG, "Run {} sender to: {}"
        , (netty::is_multicast(destSaddr.addr)
            ? "MULTICAST"
                : netty::is_broadcast(destSaddr.addr)
                ? "BROADCAST"
                : "UNICAST")
        , to_string(destSaddr));

    try {
        Sender sender;

        if (netty::is_multicast(destSaddr.addr)) {
            sender.set_multicast_interface(localAddr);
            LOGD(TAG, "Multicast interface: {}", to_string(localAddr));
        } else if (netty::is_broadcast(destSaddr.addr)) {
            sender.enable_broadcast(true);
            LOGD(TAG, "Broadcast enabled");
        }

        auto counter = maxCount;
        counter = 0;
        auto packetsSent = 0;
        bool outputLog = maxCount <= 20 && interval >= 500ms;
        char quit[] = {'Q', 'U', 'I', 'T'};

        if (!quitOnlyPacket) {
            while (++counter <= maxCount) {
                char helo[] = {'H', 'e', 'l', 'o'};
                auto send_result = sender.send_to(destSaddr, helo, sizeof(helo));

                if (send_result.status == netty::send_status::good) {
                    packetsSent++;
                } else if (send_result.status == netty::send_status::again) {
                    counter--;
                    std::this_thread::sleep_for(std::chrono::milliseconds{10});
                    continue;
                } else {
                    LOGW(TAG, "Send data status: {}", static_cast<int>(send_result.status));
                }

                if (outputLog) {
                    LOGD(TAG, "Send data (counter={}) to: {}, size={}", counter, to_string(destSaddr)
                        , send_result.n);
                }

                if (interval > std::chrono::milliseconds{0})
                    std::this_thread::sleep_for(interval);
            }

            sender.send_to(destSaddr, quit, sizeof(quit));
            LOGD(TAG, "Sent {} packets from {}", packetsSent, maxCount);
        } else {
            sender.send_to(destSaddr, quit, sizeof(quit));
        }
    } catch (netty::error const & ex) {
        LOGE(TAG, "ERROR: {}", ex.what());
    }
}
