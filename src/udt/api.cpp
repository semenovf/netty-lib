////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019 - 2022 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2021.11.06 Initial version.
//      2022.08.15 Changed signatures for startup/cleanup.
////////////////////////////////////////////////////////////////////////////////
#include "pfs/netty/error.hpp"
#include "pfs/netty/p2p/udt/api.hpp"

#if NETTY_P2P__NEW_UDT_ENABLED
#   include "newlib/udt.hpp"
#else
#   include "lib/udt.h"
#endif

namespace netty {
namespace p2p {
namespace udt {

bool api::startup (std::error_code & ec)
{
    try {
        return UDT::startup() == 0;
    } catch (CUDTException ex) {
        // Here may be exception CUDTException(1, 0, WSAGetLastError());
        // related to WSAStartup call.
        if (CUDTException{1, 0, 0}.getErrorCode() == ex.getErrorCode()) {
            ec = make_error_code(errc::system_error);
        } else {
            ec = make_error_code(errc::unexpected_error);
        }
    }

    return false;
}

void api::cleanup (std::error_code & /*ec*/)
{
    // No exception expected
    UDT::cleanup();
}

}}} // namespace netty::p2p::udt
