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
#include "pfs/netty/patterns/meshnet/heartbeat_controller.hpp"

using namespace netty::meshnet;
using socket_id = netty::posix::tcp_socket::socket_id;

TEST_CASE("basic") {
    using heartbeat_controller_t = netty::meshnet::heartbeat_controller<socket_id
        , serializer_traits_t>;

    heartbeat_controller_t heartbeat;

    // TODO
}
