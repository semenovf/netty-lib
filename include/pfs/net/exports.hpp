////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// References:
//
// Changelog:
//      2021.06.21 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#ifndef PFS_NET_STATIC_LIB
#   ifndef PFS_NET_DLL_API
#       if _MSC_VER
#           if defined(PFS_NET_DLL_EXPORTS)
#               define PFS_NET_DLL_API __declspec(dllexport)
#           else
#               define PFS_NET_DLL_API __declspec(dllimport)
#           endif
#       endif
#   endif
#else
#   define PFS_NET_DLL_API
#endif // !PFS_NET_STATIC_LIB
