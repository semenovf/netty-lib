////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.10.11 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/uuid.hpp"
#include "pfs/net/inet4_addr.hpp"
#include <unordered_map>

namespace pfs {
namespace net {
namespace p2p {

struct peer
{
    using port_type       = std::uint16_t;
    using seq_number_type = std::uint32_t;

    uuid_t          uuid;
    inet4_addr      addr;
    port_type       port {0};
    seq_number_type sn {0};
};

class peer_manager
{
    using peer_container_type = std::unordered_map<uuid_t, peer>;

private:
    peer_container_type _accepted_peers;
    peer_container_type _ready_peers;

public:
    peer_manager () = default;
    ~peer_manager () = default;

    peer_manager (peer_manager const &) = delete;
    peer_manager & operator = (peer_manager const &) = delete;

    peer_manager (peer_manager &&) = default;
    peer_manager & operator = (peer_manager &&) = default;

    void rookie_accepted (pfs::uuid_t peer_uuid
        , inet4_addr const & addr
        , peer::port_type port)
    {
        peer p;
        p.uuid = peer_uuid;
        p.addr = addr;
        p.port = port;
        _accepted_peers.emplace(peer_uuid, std::move(p));
    }
};

}}} // namespace pfs::net::p2p


