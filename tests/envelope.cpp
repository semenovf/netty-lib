////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2021 Vladislav Trifochkin
//
// License: see LICENSE file
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.10.05 Initial version.
////////////////////////////////////////////////////////////////////////////////
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "pfs/net/p2p/uuid.hpp"
#include "pfs/net/p2p/envelope.hpp"
#include <cereal/types/string.hpp>

namespace p2p {
    using output_envelope = pfs::net::p2p::output_envelope<>;
    using input_envelope  = pfs::net::p2p::input_envelope<>;
    using uuid_t          = pfs::net::p2p::uuid_t;
} // namespace p2p

TEST_CASE("Envelope") {
    int a {42};
    double b{3.14};
    p2p::uuid_t c {pfs::from_string<pfs::uuid_t>("01FH7H6YJB8XK9XNNZYR0WYDJ1")};

    p2p::output_envelope out;
    out << a << b << c;

    int a1;
    double b1;
    p2p::uuid_t c1;
    p2p::input_envelope in {out.data()};

    in >> a1 >> b1 >> c1;

    CHECK(a == a1);
    CHECK(b == b1);
    CHECK(c == c1);
}
