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
#include "serializer_traits.hpp"
#include "pfs/netty/envelope.hpp"
#include <cstdint>
#include <cstring>
#include <limits>

using envelope_t = netty::envelope<std::uint16_t, serializer_traits_t>;

TEST_CASE("basic envelope") {
    char const * chars = "ABC";
    char const * sample = "\xBE\x00\x03\x41\x42\x43\xED";

    archive_t ar;
    envelope_t ep;
    ep.pack(ar, chars, 3);

    CHECK_EQ(ar, archive_t{sample, 7});
}

TEST_CASE("parse") {
    using parser_t = typename envelope_t::parser;

    // Parse bad sequence
    {
        char const * bytes = "\000\003ABC\355"; // size = 6
        parser_t parser{bytes, 6};
        auto opt_ar = parser.next();

        CHECK(!opt_ar);
        CHECK(parser.bad());
    }

    // Parse incomplete sequence
    {
        char const * bytes = "\276\000\003ABC"; // size = 6
        parser_t parser{bytes, 6};
        auto opt_ar = parser.next();

        CHECK(!opt_ar);
        CHECK_FALSE(parser.bad());
        CHECK_EQ(parser.remain_size(), 6);
    }

    // Parse complete sequence
    {
        char const * bytes = "\276\000\003ABC\355"; // size = 7
        parser_t parser{bytes, 7};
        auto opt_ar = parser.next();

        CHECK(opt_ar);
        CHECK_FALSE(parser.bad());
        CHECK_EQ(parser.remain_size(), 0);

        CHECK_EQ(*opt_ar, archive_t{"ABC", 3});
    }

    // Parse complete sequence with extra bytes
    {
        char const * bytes = "\276\000\003ABC\355\000"; // size = 8
        parser_t parser{bytes, 8};
        auto opt_ar = parser.next();

        CHECK(opt_ar);
        CHECK_FALSE(parser.bad());
        CHECK_EQ(parser.remain_size(), 1);

        CHECK_EQ(*opt_ar, archive_t{"ABC", 3});

        opt_ar = parser.next();
        CHECK_FALSE(opt_ar);
    }

    // Parse envelope sequence
    {
        char const * bytes = "\276\000\003ABC\355\276\000\003DEF\355"; // size = 14
        parser_t parser{bytes, 14};
        auto opt_ar = parser.next();

        CHECK(opt_ar);
        CHECK_FALSE(parser.bad());
        CHECK_EQ(parser.remain_size(), 7);

        CHECK_EQ(*opt_ar, archive_t{"ABC", 3});

        opt_ar = parser.next();

        CHECK(opt_ar);
        CHECK_FALSE(parser.bad());
        CHECK_EQ(parser.remain_size(), 0);

        CHECK_EQ(*opt_ar, archive_t{"DEF", 3});
    }
}
