////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.04.16 Initial version.
////////////////////////////////////////////////////////////////////////////////
#if _MSC_VER
#   define COLOR(x)
#else
#   define COLOR(x) "\033[" #x "m"
#endif

#define BLACK     COLOR(0;30)
#define DGRAY     COLOR(1;30)
#define BLUE      COLOR(0;34)
#define LBLUE     COLOR(1;34)
#define MAGENTA   COLOR(0;35)
#define LMAGENTA  COLOR(1;35)
#define LGRAY     COLOR(0;37)
#define GREEN     COLOR(0;32)
#define LGREEN    COLOR(1;32)
#define RED       COLOR(0;31)
#define LRED      COLOR(1;31)
#define CYAN      COLOR(0;36)
#define LCYAN     COLOR(1;36)
#define WHITE     COLOR(1;37)
#define ORANGE    COLOR(0;33)
#define YELLOW    COLOR(1;33)
#define END_COLOR COLOR(0)
