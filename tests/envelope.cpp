////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2021 Vladislav Trifochkin
//
// License: see LICENSE file
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2021.10.05 Initial version.
////////////////////////////////////////////////////////////////////////////////
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "pfs/netty/p2p/universal_id.hpp"
#include "pfs/netty/p2p/envelope.hpp"
#include <cereal/types/string.hpp>

namespace p2p {
    using output_envelope = netty::p2p::output_envelope<>;
    using input_envelope  = netty::p2p::input_envelope<>;
    using universal_id    = netty::p2p::universal_id;
} // namespace p2p

TEST_CASE("Envelope") {
    int a {42};
    double b{3.14};
    p2p::universal_id c = "01FH7H6YJB8XK9XNNZYR0WYDJ1"_uuid;

    p2p::output_envelope out;
    out << a << b << c;

    int a1;
    double b1;
    p2p::universal_id c1;
    auto data = out.data();
    p2p::input_envelope in {data};

    in >> a1 >> b1 >> c1;

    CHECK(a == a1);
    CHECK(b == b1);
    CHECK(c == c1);
}
