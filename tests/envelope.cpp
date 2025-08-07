////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.08.06 Initial version.
////////////////////////////////////////////////////////////////////////////////
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "pfs/netty/envelope.hpp"
#include <pfs/lorem/utils.hpp>
#include <limits>

using envelope_t = netty::envelope16_t;

TEST_CASE("empty envelope") {
    envelope_t env{0};

    CHECK_EQ(env.payload(), nullptr);
    CHECK_EQ(env.payload_size(), 0);
    CHECK(env.empty());
}

TEST_CASE("basic envelope") {
    envelope_t env{3};

    CHECK_NE(env.payload(), nullptr);
    CHECK_EQ(env.payload_size(), 3);

    char const * chars = "ABC";

    env.copy(chars, 3);

    CHECK_EQ(env.payload()[0], 'A');
    CHECK_EQ(env.payload()[1], 'B');
    CHECK_EQ(env.payload()[2], 'C');
}

TEST_CASE("reduce envelope") {
    envelope_t env{3};

    env.copy("ABC", 3);

    CHECK_EQ(env.payload_size(), 3);
    CHECK_EQ(env.payload()[0], 'A');
    CHECK_EQ(env.payload()[1], 'B');
    CHECK_EQ(env.payload()[2], 'C');

    env.copy("CD", 2);

    CHECK_EQ(env.payload_size(), 2);
    CHECK_EQ(env.payload()[0], 'C');
    CHECK_EQ(env.payload()[1], 'D');
}

TEST_CASE("enlarge envelope") {
    envelope_t env{3};

    env.copy("ABC", 3);

    CHECK_EQ(env.payload_size(), 3);
    CHECK_EQ(env.payload()[0], 'A');
    CHECK_EQ(env.payload()[1], 'B');
    CHECK_EQ(env.payload()[2], 'C');

    env.copy("CDEF", 4);

    CHECK_EQ(env.payload_size(), 4);
    CHECK_EQ(env.payload()[0], 'C');
    CHECK_EQ(env.payload()[1], 'D');
    CHECK_EQ(env.payload()[2], 'E');
    CHECK_EQ(env.payload()[3], 'F');
}

TEST_CASE("clear envelope") {
    envelope_t env{3};

    env.copy("ABC", 3);

    CHECK_EQ(env.payload_size(), 3);
    CHECK_EQ(env.payload()[0], 'A');
    CHECK_EQ(env.payload()[1], 'B');
    CHECK_EQ(env.payload()[2], 'C');

    env.copy(nullptr, 0);

    CHECK(env.empty());
}

TEST_CASE("copy constructor") {
    envelope_t env{2};
    env.copy("AB", 2);

    envelope_t another_env {env};

    CHECK_EQ(env.payload_size(), 2);
    CHECK_EQ(env.payload()[0], 'A');
    CHECK_EQ(env.payload()[1], 'B');

    CHECK_EQ(another_env.payload_size(), 2);
    CHECK_EQ(another_env.payload()[0], 'A');
    CHECK_EQ(another_env.payload()[1], 'B');
}

TEST_CASE("copy operator") {
    envelope_t env{2};
    env.copy("AB", 2);

    envelope_t another_env;
    CHECK(another_env.empty());

    another_env = env;

    CHECK_EQ(env.payload_size(), 2);
    CHECK_EQ(env.payload()[0], 'A');
    CHECK_EQ(env.payload()[1], 'B');

    CHECK_EQ(another_env.payload_size(), 2);
    CHECK_EQ(another_env.payload()[0], 'A');
    CHECK_EQ(another_env.payload()[1], 'B');
}

TEST_CASE("move constructor") {
    envelope_t env{2};
    env.copy("AB", 2);

    envelope_t another_env {std::move(env)};

    CHECK_EQ(another_env.payload_size(), 2);
    CHECK_EQ(another_env.payload()[0], 'A');
    CHECK_EQ(another_env.payload()[1], 'B');
}

TEST_CASE("move operator") {
    envelope_t env{2};
    env.copy("AB", 2);

    envelope_t another_env;
    CHECK(another_env.empty());

    another_env = std::move(env);

    CHECK_EQ(another_env.payload_size(), 2);
    CHECK_EQ(another_env.payload()[0], 'A');
    CHECK_EQ(another_env.payload()[1], 'B');
}

TEST_CASE("parse") {
    // Parse bad sequence
    {
        char const * bytes = "\000\003ABC\355"; // size = 8
        envelope_t::parser parser{bytes, 6};
        auto oenv = parser.next();

        CHECK(!oenv);
        CHECK(parser.bad());
    }

    // Parse incomplete sequence
    {
        char const * bytes = "\276\000\003ABC"; // size = 6
        envelope_t::parser parser{bytes, 6};
        auto oenv = parser.next();

        CHECK(!oenv);
        CHECK_FALSE(parser.bad());
        CHECK_EQ(parser.remain_size(), 6);
    }

    // Parse complete sequence
    {
        char const * bytes = "\276\000\003ABC\355"; // size = 7
        envelope_t::parser parser{bytes, 7};
        auto oenv = parser.next();

        CHECK(oenv);
        CHECK_FALSE(parser.bad());
        CHECK_EQ(parser.remain_size(), 0);

        CHECK_EQ(oenv->payload_size(), 3);
        CHECK_EQ(oenv->payload()[0], 'A');
        CHECK_EQ(oenv->payload()[1], 'B');
        CHECK_EQ(oenv->payload()[2], 'C');
    }

    // Parse complete sequence with extra bytes
    {
        char const * bytes = "\276\000\003ABC\355\000"; // size = 8
        envelope_t::parser parser{bytes, 8};
        auto oenv = parser.next();

        CHECK(oenv);
        CHECK_FALSE(parser.bad());
        CHECK_EQ(parser.remain_size(), 1);

        CHECK_EQ(oenv->payload_size(), 3);
        CHECK_EQ(oenv->payload()[0], 'A');
        CHECK_EQ(oenv->payload()[1], 'B');
        CHECK_EQ(oenv->payload()[2], 'C');
    }
}

TEST_CASE("large envelope") {
    auto sample_size = (std::numeric_limits<std::uint16_t>::max)();
    auto sample_data = lorem::random_binary_data(sample_size);

    envelope_t env {sample_size};
    env.copy(sample_data.data(), sample_data.size());

    CHECK_EQ(env.payload_size(), sample_size);
}
