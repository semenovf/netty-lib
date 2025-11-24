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
#include "pfs/netty/patterns/meshnet/alive_controller.hpp"
#include <pfs/universal_id.hpp>
#include <pfs/universal_id_hash.hpp>

using namespace netty::meshnet;
using node_id = pfs::universal_id;

TEST_CASE("basic") {
    using alive_controller_t = netty::meshnet::alive_controller<node_id, serializer_traits_t>;

    node_id id = pfs::generate_uuid();
    std::chrono::seconds exp_timeout = std::chrono::seconds{15};
    std::chrono::seconds interval = std::chrono::seconds{5};
    std::chrono::milliseconds looping_interval = std::chrono::milliseconds{2500};

    alive_controller_t ac {id, exp_timeout, interval, looping_interval};

    // TODO
}
