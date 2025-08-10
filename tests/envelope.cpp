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
#include <limits>

using envelope_t = netty::envelope16be_t;

TEST_CASE("empty envelope") {
    std::vector<char> buf;
    envelope_t env{buf};

    CHECK(buf.empty());
}

TEST_CASE("basic envelope") {
    std::vector<char> buf;
    envelope_t env{buf};

    CHECK(buf.empty());

    char const * chars = "ABC";

    env.pack(chars, 3);

    CHECK_EQ(buf, std::vector<char>{'\xBE', '\x00', '\x03', '\x41', '\x42', '\x43', '\xED'});
}

TEST_CASE("copy constructor") {
    CHECK(!std::is_copy_constructible<envelope_t>::value);
}

TEST_CASE("copy assigment operator") {
    CHECK(!std::is_copy_assignable<envelope_t>::value);
}

TEST_CASE("move constructor") {
    CHECK(std::is_move_constructible<envelope_t>::value);

    std::vector<char> buf;
    envelope_t env{buf};

    envelope_t another_env {std::move(env)};
    char const * chars = "ABC";

    another_env.pack(chars, 3);
    CHECK_EQ(buf, std::vector<char>{'\xBE', '\x00', '\x03', '\x41', '\x42', '\x43', '\xED'});
}

TEST_CASE("move assignment operator") {
    CHECK(std::is_move_assignable<envelope_t>::value);

    std::vector<char> buf;
    envelope_t env{buf};

    std::vector<char> tmp;
    envelope_t another_env {tmp};
    another_env = std::move(env);

    char const * chars = "ABC";

    another_env.pack(chars, 3);
    CHECK_EQ(buf, std::vector<char>{'\xBE', '\x00', '\x03', '\x41', '\x42', '\x43', '\xED'});
}

TEST_CASE("parse") {
    // Parse bad sequence
    {
        char const * bytes = "\000\003ABC\355"; // size = 8
        envelope_t::parser parser{bytes, 6};
        auto obuf = parser.next();

        CHECK(!obuf);
        CHECK(parser.bad());
    }

    // Parse incomplete sequence
    {
        char const * bytes = "\276\000\003ABC"; // size = 6
        envelope_t::parser parser{bytes, 6};
        auto obuf = parser.next();

        CHECK(!obuf);
        CHECK_FALSE(parser.bad());
        CHECK_EQ(parser.remain_size(), 6);
    }

    // Parse complete sequence
    {
        char const * bytes = "\276\000\003ABC\355"; // size = 7
        envelope_t::parser parser{bytes, 7};
        auto obuf = parser.next();

        CHECK(obuf);
        CHECK_FALSE(parser.bad());
        CHECK_EQ(parser.remain_size(), 0);

        CHECK_EQ(*obuf, std::vector<char>{'A', 'B', 'C'});
    }

    // Parse complete sequence with extra bytes
    {
        char const * bytes = "\276\000\003ABC\355\000"; // size = 8
        envelope_t::parser parser{bytes, 8};
        auto obuf = parser.next();

        CHECK(obuf);
        CHECK_FALSE(parser.bad());
        CHECK_EQ(parser.remain_size(), 1);

        CHECK_EQ(*obuf, std::vector<char>{'A', 'B', 'C'});

        obuf = parser.next();
        CHECK_FALSE(obuf);
    }

    // Parse envelope sequence
    {
        char const * bytes = "\276\000\003ABC\355\276\000\003DEF\355"; // size = 14
        envelope_t::parser parser{bytes, 14};
        auto obuf = parser.next();

        CHECK(obuf);
        CHECK_FALSE(parser.bad());
        CHECK_EQ(parser.remain_size(), 7);

        CHECK_EQ(*obuf, std::vector<char>{'A', 'B', 'C'});

        obuf = parser.next();

        CHECK(obuf);
        CHECK_FALSE(parser.bad());
        CHECK_EQ(parser.remain_size(), 0);

        CHECK_EQ(*obuf, std::vector<char>{'D', 'E', 'F'});
    }
}
