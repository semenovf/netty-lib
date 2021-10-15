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
#include "pfs/uuid.hpp"
#include "pfs/net/p2p/legacy/envelope.hpp"
#include <cereal/types/string.hpp>

using namespace pfs::net;
using uuid_t            = pfs::uuid_t;
using output_envelope_t = p2p::output_envelope<>;
using input_envelope_t  = p2p::input_envelope<>;

TEST_CASE("Envelope") {
    int a {42};
    double b{3.14};
    uuid_t c {pfs::from_string<uuid_t>("01FH7H6YJB8XK9XNNZYR0WYDJ1")};

    output_envelope_t oenvlp;
    oenvlp << a << b << c << p2p::seal;

    int a1;
    double b1;
    uuid_t c1;
    input_envelope_t ienvlp {oenvlp.data()};

    ienvlp >> a1 >> b1 >> c1 >> p2p::unseal;

    CHECK_EQ(a, a1);
    CHECK_EQ(b, b1);
    CHECK_EQ(c, c1);
    CHECK(ienvlp.success());

    CHECK_EQ(oenvlp.crc32(), ienvlp.crc32());
}
