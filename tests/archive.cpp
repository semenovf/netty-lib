////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.11.19 Initial version.
////////////////////////////////////////////////////////////////////////////////
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "serializer_traits.hpp"
#include <algorithm>

constexpr char const * ABC = "ABC";

TEST_CASE("constructors") {
    archive_t ar1;

    CHECK(ar1.empty());

    archive_t ar2 {ABC, 3};
    CHECK_EQ(ar2.size(), 3);

    archive_t ar3 {std::move(ar2)};
    CHECK_EQ(ar3.size(), 3);
    CHECK(ar2.empty());
}

TEST_CASE("assign_ops") {
    archive_t ar1 {ABC, 3};
    CHECK_EQ(ar1.size(), 3);

    archive_t ar2;
    ar2 = std::move(ar1);
    CHECK_EQ(ar2.size(), 3);
    CHECK(ar1.empty());
}

TEST_CASE("data") {
    archive_t ar {ABC, 3};
    CHECK_EQ(ar.size(), 3);
    CHECK_EQ(ar.data()[0], 'A');
    CHECK_EQ(ar.data()[1], 'B');
    CHECK_EQ(ar.data()[2], 'C');
}

TEST_CASE("append") {
    archive_t ar;
    ar.append(ABC, 3);
    ar.append('x');

    CHECK_EQ(ar.size(), 4);
    CHECK_EQ(ar.data()[0], 'A');
    CHECK_EQ(ar.data()[1], 'B');
    CHECK_EQ(ar.data()[2], 'C');
    CHECK_EQ(ar.data()[3], 'x');
}

TEST_CASE("clear") {
    archive_t ar {ABC, 3};

    CHECK_EQ(ar.size(), 3);

    ar.clear();
    CHECK(ar.empty());
}

TEST_CASE("erase") {
    {
        archive_t ar {ABC, 3};
        ar.erase(1, 2);
        CHECK_EQ(ar.size(), 1);
        CHECK_EQ(ar.data()[0], 'A');
    }

    {
        archive_t ar {ABC, 3};
        CHECK_THROWS_AS(ar.erase(1, 3), std::range_error);
    }
}

TEST_CASE("erase_front") {
    {
        archive_t ar {ABC, 3};
        CHECK_EQ(ar.size(), 3);
        CHECK_EQ(ar.data()[0], 'A');

        ar.erase_front(1);
        CHECK_EQ(ar.size(), 2);
        CHECK_EQ(ar.data()[0], 'B');

        ar.erase_front(2);
        CHECK_EQ(ar.size(), 0);
    }

    {
        archive_t ar {ABC, 3};
        CHECK_THROWS_AS(ar.erase_front(4), std::range_error);
    }
}

TEST_CASE("resize and copy") {
    std::string abc = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    archive_t ar;

    CHECK_EQ(ar.size(), 0);

    ar.resize(abc.size());

    CHECK_EQ(ar.size(), abc.size());

    std::size_t step = 4;

    for (std::size_t i = 0; i < abc.size(); i += std::min(step, abc.size() - i)) {
        auto n = std::min(step, abc.size() - i);
        ar.copy(abc.data() + i, n, i);
    }

    CHECK_EQ(abc, std::string(ar.data(), ar.size()));
}
