////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.03 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "pfs/assert.hpp"

#if _MSC_VER
#   define WIN32_LEAN_AND_MEAN
#   include <winsock2.h>
#endif

namespace netty {

//
// NOTE [Static Initialization Order Fiasco](https://en.cppreference.com/w/cpp/language/siof)
//

#if _MSC_VER

// https://learn.microsoft.com/en-us/windows/win32/api/winsock/nf-winsock-wsastartup

class static_initializer
{
public:
    static_initializer () {
        WSADATA wsa_data;
        auto version_requested = MAKEWORD(2, 2);

        auto rc = WSAStartup(version_requested, & wsa_data);
        PFS__TERMINATE(rc != 0, "WSAStartup failed: the Winsock 2.2 or newer dll was not found");
    }

    ~static_initializer () {
        WSACleanup();
    }
};

static static_initializer __static_initializer{};

#endif // _MSC_VER

} // namespace netty
