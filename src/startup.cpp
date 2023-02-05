////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.06 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "pfs/assert.hpp"
#include "pfs/netty/startup.hpp"
#include <atomic>

#if _MSC_VER
#   define WIN32_LEAN_AND_MEAN
#   include <winsock2.h>
#endif

#if NETTY__UDT_ENABLED
#   include "udt/newlib/udt.hpp"
#endif

namespace netty {

//
// NOTE [Static Initialization Order Fiasco](https://en.cppreference.com/w/cpp/language/siof)
//

// https://learn.microsoft.com/en-us/windows/win32/api/winsock/nf-winsock-wsastartup

static std::atomic_int startup_counter {0};

void startup ()
{
    if (startup_counter.fetch_add(1) == 0) {
#if _MSC_VER
        WSADATA wsa_data;
        auto version_requested = MAKEWORD(2, 2);

        auto rc = WSAStartup(version_requested, & wsa_data);
        PFS__TERMINATE(rc == 0, "WSAStartup failed: the Winsock 2.2 or newer dll was not found");
#endif // _MSC_VER

#if NETTY__UDT_ENABLED
        try {
            UDT::startup_context ctx;

            ctx.state_changed_callback = [] (UDTSOCKET) {
                //_socket_state_changed_buffer.push(sid);
            };

            UDT::startup(std::move(ctx));
        } catch (CUDTException ex) {
            // Here may be exception CUDTException(1, 0, WSAGetLastError());
            // related to WSAStartup call.
            if (CUDTException{1, 0, 0}.getErrorCode() == ex.getErrorCode()) {
                PFS__TERMINATE(false, ex.getErrorMessage());
            } else {
                // Unexpected error
                PFS__TERMINATE(false, ex.getErrorMessage());
            }
        }
#endif
    }
}

void cleanup ()
{
    if (startup_counter.fetch_sub(1) == 1) {
#if _MSC_VER
        WSACleanup();
#endif // _MSC_VER

#if NETTY__UDT_ENABLED
        UDT::cleanup();
#endif
    }
}

} //namespace netty
