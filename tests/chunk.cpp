////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.09.08 Initial version.
////////////////////////////////////////////////////////////////////////////////
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "pfs/netty/chunk.hpp"
#include "pfs/netty/default_archive_traits.hpp"

using archive_traits = netty::archive_traits<std::vector<char>>;
using archive_t = archive_traits::archive_type;
using chunk_t = netty::chunk<archive_traits>;

static char const * ABC = "ABC";

TEST_CASE("contructors") {
    chunk_t chunk1;

    CHECK(chunk1.empty());

    chunk_t chunk2 {archive_traits::make_archive(ABC, std::strlen(ABC))};
    CHECK_EQ(chunk2.size(), 3);

    chunk_t chunk3 {chunk2};
    CHECK_EQ(chunk3.size(), 3);

    chunk_t chunk4 {std::move(chunk3)};
    CHECK_EQ(chunk4.size(), 3);
}

TEST_CASE("assign operators") {
    chunk_t chunk1 {archive_traits::make_archive(ABC, std::strlen(ABC))};
    CHECK_EQ(chunk1.size(), 3);

    chunk_t chunk2;
    chunk2 = chunk1;
    CHECK_EQ(chunk2.size(), 3);

    chunk_t chunk3;
    chunk3 = std::move(chunk2);
    CHECK_EQ(chunk3.size(), 3);
}

TEST_CASE("iterators") {
    chunk_t chunk {archive_traits::make_archive(ABC, std::strlen(ABC))};

    auto pos = chunk.begin();
    CHECK_EQ(*pos++, 'A');
    CHECK_EQ(*pos++, 'B');
    CHECK_EQ(*pos++, 'C');
    CHECK_EQ(pos, chunk.end());
}

// TEST_CASE("erase") {
//     netty::chunk chunk {std::vector<char>{'A', 'B', 'C'}};
//
//     CHECK_EQ(chunk.size(), 3);
//
//     chunk.erase(chunk.begin(), chunk.begin() + 1);
//     CHECK_EQ(chunk.size(), 2);
//
//     auto pos = chunk.begin();
//     CHECK_EQ(*pos++, 'B');
//     CHECK_EQ(*pos++, 'C');
//     CHECK_EQ(pos, chunk.end());
//
//     chunk = std::move(std::vector<char>{'A', 'B', 'C'});
//     CHECK_EQ(chunk.size(), 3);
//
//     CHECK_THROWS_AS(chunk.erase(chunk.begin(), chunk.begin() + chunk.size() + 1), std::range_error);
//
//     chunk.erase(chunk.begin(), chunk.end());
//     CHECK(chunk.empty());
// }
//
// TEST_CASE("clear") {
//     netty::chunk chunk {std::vector<char>{'A', 'B', 'C'}};
//
//     CHECK_EQ(chunk.size(), 3);
//
//     chunk.clear();
//     CHECK(chunk.empty());
// }
