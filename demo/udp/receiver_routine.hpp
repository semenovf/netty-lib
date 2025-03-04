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
#include "pfs/netty/reader_poller.hpp"
#include <map>
#include <cstring>

#if NETTY__EPOLL_ENABLED
#   include "pfs/netty/linux/epoll_poller.hpp"
    using receiver_poller_type = netty::reader_poller<netty::linux_os::epoll_poller>;
#elif NETTY__POLL_ENABLED
#   include "pfs/netty/posix/poll_poller.hpp"
    using receiver_poller_type = netty::reader_poller<netty::posix::poll_poller>;
#elif NETTY__SELECT_ENABLED
#   include "pfs/netty/posix/select_poller.hpp"
    using receiver_poller_type = netty::reader_poller<netty::posix::select_poller>;
#endif

// `local_addr` for multicast only
template <typename Receiver>
void run_receiver (netty::socket4_addr const & src_saddr, netty::inet4_addr local_addr
    , bool outputLog = false)
{
    LOGD(TAG, "Run {} receiver on: {}"
        , (netty::is_multicast(src_saddr.addr)
            ? "MULTICAST"
                : netty::is_broadcast(src_saddr.addr)
                ? "BROADCAST"
                : "UNICAST")
        , to_string(src_saddr));

    receiver_poller_type poller;

    std::uint32_t packetsReceived = 0;
    bool finish = false;
    Receiver receiver;

    try {
        if (netty::is_multicast(src_saddr.addr) || netty::is_broadcast(src_saddr.addr))
            receiver = Receiver{src_saddr, local_addr};
        else
            receiver = Receiver{src_saddr};

        poller.on_ready_read = [& receiver, & finish, & packetsReceived, outputLog] (receiver_poller_type::socket_id /*sock*/) {
            char buffer[5];
            std::memset(buffer, 0, sizeof(buffer));

            netty::socket4_addr sender_addr;
            auto n = receiver.recv_from(buffer, sizeof(buffer), & sender_addr);

            if (n > 0) {
                if (outputLog)
                    LOGD(TAG, "Received data from: {}: {}", to_string(sender_addr), buffer);

                if (buffer[0] == 'Q' && buffer[1] == 'U' && buffer[2] == 'I' && buffer[3] == 'T')
                    finish = true;
                else
                    packetsReceived++;
            }
        };

        std::chrono::seconds poller_timeout {1};

        poller.add(receiver.id());

        while (!finish) {
            poller.poll(poller_timeout);
        }

        LOGD(TAG, "Received {} packets successfully", packetsReceived);
    } catch (netty::error const & ex) {
        LOGE(TAG, "ERROR: {}", ex.what());
    }
}
