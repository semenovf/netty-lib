////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.11.22 Initial version.
////////////////////////////////////////////////////////////////////////////////
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../../doctest.h"
#include "../../serializer_traits.hpp"
#include "pfs/netty/patterns/priority_tracker.hpp"
#include "pfs/netty/patterns/meshnet/priority_writer_queue.hpp"

using namespace netty::meshnet;

struct distribution
{
    std::array<std::size_t, 7> distrib {7, 6, 5, 4, 3, 2, 1};

    static constexpr std::size_t SIZE = 7;

    constexpr std::size_t operator [] (std::size_t i) const noexcept
    {
        return distrib[i];
    }
};

using priority_tracker_t = netty::priority_tracker<distribution>;
using priority_writer_queue_t = priority_writer_queue<priority_tracker_t, serializer_traits_t>;

TEST_CASE("basic") {
    priority_writer_queue_t q;

    // TODO
}

