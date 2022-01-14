////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019 - 2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.11.06 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "pfs/netty/p2p/udt/api.hpp"

#if NETTY_P2P__NEW_UDT_ENABLED
#   include "newlib/udt.hpp"
#else
#   include "lib/udt.h"
#endif

namespace netty {
namespace p2p {
namespace udt {

bool api::startup ()
{
    return UDT::startup() == 0;
}

void api::cleanup ()
{
    UDT::cleanup();
}

}}} // namespace netty::p2p::udt
