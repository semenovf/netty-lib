////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.11.22 Initial version.
////////////////////////////////////////////////////////////////////////////////
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../doctest.h"
#include "../serializer_traits.hpp"
#include "pfs/netty/posix/tcp_socket.hpp"
#include "pfs/netty/patterns/meshnet/single_link_handshake.hpp"
#include <pfs/universal_id.hpp>
#include <pfs/universal_id_pack.hpp>

using namespace netty::meshnet;
using node_id = pfs::universal_id;
using socket_id = netty::posix::tcp_socket::socket_id;

TEST_CASE("single link") {
    using handshake_controller_t = netty::meshnet::single_link_handshake<socket_id
        , node_id
        , serializer_traits_t>;

    auto id = pfs::generate_uuid();
    bool is_gateway = true; // value doesn't matter
    handshake_controller_t handshake {id, is_gateway};

    // TODO
}

TEST_CASE("dual link") {
    // TODO
}
