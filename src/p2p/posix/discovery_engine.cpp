////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.17 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "pfs/netty/p2p/posix/discovery_engine.hpp"
#include <algorithm>

namespace netty {
namespace p2p {
namespace posix {

discovery_engine::discovery_engine () = default;
discovery_engine::~discovery_engine () = default;

void discovery_engine::add_listener (socket4_addr src_saddr, inet4_addr local_addr)
{
    netty::posix::udp_receiver receiver;

    if (is_multicast(src_saddr.addr) || is_broadcast(src_saddr.addr))
        receiver = netty::posix::udp_receiver{src_saddr, local_addr};
    else
        receiver = netty::posix::udp_receiver{src_saddr};

    _poller.ready_read = [this] (poller_type::native_socket_type sock) {
        auto pos = _listeners.find(sock);

        if (pos != _listeners.end()) {
            auto & receiver = pos->second;
            auto bytes_available = receiver.available();

            if (bytes_available > 0) {
                socket4_addr sender_saddr;
                std::vector<char> buffer;

                buffer.resize(static_cast<int>(bytes_available));
                auto bytes_read = receiver.recv_from(buffer.data(), buffer.size(), & sender_saddr);

                if (bytes_read > 0)
                    this->data_ready(sender_saddr, std::move(buffer));
            }
        }
    };

    _poller.add(receiver.native());
    _listeners.emplace(receiver.native(), std::move(receiver));
}

void discovery_engine::add_target (socket4_addr dest_saddr, inet4_addr local_addr)
{
    netty::posix::udp_sender sender;

    if (netty::is_multicast(dest_saddr.addr)) {
        sender.set_multicast_interface(local_addr);
    } else if (netty::is_broadcast(dest_saddr.addr)) {
        sender.enable_broadcast(true);
    }

    _targets.emplace_back(std::make_pair(dest_saddr, std::move(sender)));
}

void discovery_engine::poll (std::chrono::milliseconds timeout)
{
    _poller.poll(timeout);
}

std::streamsize discovery_engine::send (socket4_addr dest_saddr, char const * data
    , std::size_t size, netty::error * perr)
{
    for (auto & p: _targets) {
        if (dest_saddr == p.first) {
            return p.second.send_to(dest_saddr, data, size, perr);
        }
    }

    return 0;
}

}}} // namespace netty::p2p::posix
