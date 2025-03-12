////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.03.11 Initial version.
////////////////////////////////////////////////////////////////////////////////
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
// #include <pfs/assert.hpp>
// #include <pfs/log.hpp>
#include <pfs/standard_paths.hpp>
#include <pfs/lorem/lorem_ipsum.hpp>
#include <pfs/netty/patterns/serializer_traits.hpp>
#include <pfs/netty/patterns/meshnet/universal_id_traits.hpp>
#include <pfs/netty/patterns/meshnet/routing_table_persistent.hpp>
#include <pfs/netty/patterns/meshnet/routing_table_binary_storage.hpp>
#include <memory>

using namespace netty::patterns;
namespace fs = pfs::filesystem;

using routing_table_storage_t = meshnet::routing_table_binary_storage<meshnet::universal_id_traits>;
using routing_table_t = meshnet::routing_table_persistent<meshnet::universal_id_traits
    , netty::patterns::default_serializer_traits_t, routing_table_storage_t>;

TEST_CASE("meshnet routing table") {
    pfs::universal_id nodeid = "01JJP9Y5YTSC57FZZZERMASTER"_uuid;
    auto rtab_path = fs::standard_paths::temp_folder() / "meshnet_routing_table_test.bin";

    auto gw1 = pfs::generate_uuid();
    auto gw2 = pfs::generate_uuid();
    auto gw3 = pfs::generate_uuid();
    auto gw4 = pfs::generate_uuid();

    auto n1 = pfs::generate_uuid();
    auto n2 = pfs::generate_uuid();
    auto n3 = pfs::generate_uuid();
    auto n4 = pfs::generate_uuid();

    auto hops1 = 1;
    auto hops2 = 2;
    auto hops3 = 3;
    auto hops4 = 4;

    {
        if (fs::exists(rtab_path))
            fs::remove(rtab_path);

        REQUIRE_FALSE(fs::exists(rtab_path));

        auto rtab = std::make_unique<routing_table_t>(nodeid, std::make_unique<routing_table_storage_t>(rtab_path));

        rtab->append_gateway(gw1);
        rtab->append_gateway(gw2);
        rtab->append_gateway(gw3);
        rtab->append_gateway(gw4);

        rtab->add_route(n1, gw1, hops1);
        rtab->add_route(n2, gw2, hops2);
        rtab->add_route(n3, gw3, hops3);
        rtab->add_route(n4, gw4, hops4);
        rtab->add_route(n4, gw3, hops4 + 1);
    }

    {
        REQUIRE(fs::exists(rtab_path));
        auto rtab = std::make_unique<routing_table_t>(nodeid, std::make_unique<routing_table_storage_t>(rtab_path));

        CHECK_EQ(rtab->default_gateway(), gw1);

        CHECK_EQ(rtab->gateway_for(n1), gw1);
        CHECK_EQ(rtab->gateway_for(n2), gw2);
        CHECK_EQ(rtab->gateway_for(n3), gw3);
        CHECK_EQ(rtab->gateway_for(n4), gw4);
    }
}
