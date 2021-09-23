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

#ifndef PFS_NET_LIB_STATIC
#   ifndef PFS_NET_LIB_DLL_EXPORT
#       if _MSC_VER
#           if defined(PFS_NET_LIB_EXPORTS)
#               define PFS_NET_LIB_DLL_EXPORT __declspec(dllexport)
#           else
#               define PFS_NET_LIB_DLL_EXPORT __declspec(dllimport)
#           endif
#       else
#           define PFS_NET_LIB_DLL_EXPORT
#       endif
#   endif
#else
#   define PFS_NET_LIB_DLL_EXPORT
#endif // !PFS_NET_LIB_STATIC
