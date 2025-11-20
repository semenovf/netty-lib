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
#include "archive_and_serializer_traits.hpp"
#include "pfs/netty/envelope.hpp"
#include <cstdint>
#include <cstring>
#include <limits>

using envelope_t = netty::envelope<std::uint16_t, serializer_traits_t>;

TEST_CASE("basic envelope") {
    char const * chars = "ABC";
    char const * sample = "\xBE\x00\x03\x41\x42\x43\xED";

    archive_t buf;
    envelope_t ep;
    ep.pack(buf, chars, 3);

    CHECK_EQ(buf, archive_traits_t::make(sample, 7));
}

TEST_CASE("parse") {
    using parser_t = typename envelope_t::parser;

    // Parse bad sequence
    {
        char const * bytes = "\000\003ABC\355"; // size = 6
        parser_t parser{bytes, 6};
        auto obuf = parser.next();

        CHECK(!obuf);
        CHECK(parser.bad());
    }

    // Parse incomplete sequence
    {
        char const * bytes = "\276\000\003ABC"; // size = 6
        parser_t parser{bytes, 6};
        auto obuf = parser.next();

        CHECK(!obuf);
        CHECK_FALSE(parser.bad());
        CHECK_EQ(parser.remain_size(), 6);
    }

    // Parse complete sequence
    {
        char const * bytes = "\276\000\003ABC\355"; // size = 7
        parser_t parser{bytes, 7};
        auto obuf = parser.next();

        CHECK(obuf);
        CHECK_FALSE(parser.bad());
        CHECK_EQ(parser.remain_size(), 0);

        CHECK_EQ(*obuf, archive_traits_t::make("ABC", 3));
    }

    // Parse complete sequence with extra bytes
    {
        char const * bytes = "\276\000\003ABC\355\000"; // size = 8
        parser_t parser{bytes, 8};
        auto obuf = parser.next();

        CHECK(obuf);
        CHECK_FALSE(parser.bad());
        CHECK_EQ(parser.remain_size(), 1);

        CHECK_EQ(*obuf, archive_traits_t::make("ABC", 3));

        obuf = parser.next();
        CHECK_FALSE(obuf);
    }

    // Parse envelope sequence
    {
        char const * bytes = "\276\000\003ABC\355\276\000\003DEF\355"; // size = 14
        parser_t parser{bytes, 14};
        auto obuf = parser.next();

        CHECK(obuf);
        CHECK_FALSE(parser.bad());
        CHECK_EQ(parser.remain_size(), 7);

        CHECK_EQ(*obuf, archive_traits_t::make("ABC", 3));

        obuf = parser.next();

        CHECK(obuf);
        CHECK_FALSE(parser.bad());
        CHECK_EQ(parser.remain_size(), 0);

        CHECK_EQ(*obuf, archive_traits_t::make("DEF", 3));
    }
}
