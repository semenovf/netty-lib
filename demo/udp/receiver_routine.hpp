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
#include "pfs/netty/regular_poller.hpp"
#include "pfs/netty/posix/poll_poller.hpp"
#include "pfs/netty/posix/udp_receiver.hpp"
#include <map>
#include <cstring>

using receiver_poller_type = netty::regular_poller<netty::posix::poll_poller>;

// `local_addr` for multicast only
void run_receiver (netty::socket4_addr const & src_saddr, netty::inet4_addr local_addr)
{
    LOGD(TAG, "Run {} receiver on: {}"
        , (netty::is_multicast(src_saddr.addr)
            ? "MULTICAST"
                : netty::is_broadcast(src_saddr.addr)
                ? "BROADCAST"
                : "UNICAST")
        , to_string(src_saddr));


    receiver_poller_type poller;

    bool finish = false;
    netty::posix::udp_socket receiver;

    try {
        if (netty::is_multicast(src_saddr.addr) || netty::is_broadcast(src_saddr.addr))
            receiver = netty::posix::udp_receiver{src_saddr, local_addr};
        else
            receiver = netty::posix::udp_receiver{src_saddr};

        poller.ready_read = [& receiver, & finish, & src_saddr] (receiver_poller_type::native_socket_type /*sock*/) {
            char buffer[5];
            ::memset(buffer, 0, sizeof(buffer));

            auto n = receiver.recv_from(src_saddr, buffer, sizeof(buffer));

            if (n > 0) {
                LOGD(TAG, "Received data: {}", buffer);

                if (buffer[0] == 'Q' && buffer[1] == 'U'
                        && buffer[2] == 'I' && buffer[3] == 'T') {
                    finish = true;
                }
            }
        };

        std::chrono::seconds poller_timeout {1};

        poller.add(receiver.native());

        while (!finish) {
            poller.poll(poller_timeout);
        }
    } catch (netty::error const & ex) {
        LOGE(TAG, "ERROR: {}", ex.what());
    }
}
