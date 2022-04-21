////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2021 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// References:
//
// Changelog:
//      2021.06.21 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#ifndef NETTY__STATIC
#   ifndef NETTY__EXPORT
#       if _MSC_VER
#           if defined(NETTY__EXPORTS)
#               define NETTY__EXPORT __declspec(dllexport)
#           else
#               define NETTY__EXPORT __declspec(dllimport)
#           endif
#       else
#           define NETTY__EXPORT
#       endif
#   endif
#else
#   define NETTY__EXPORT
#endif // !NETTY__STATIC
