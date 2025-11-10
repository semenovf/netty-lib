////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.11.10 Initial version.
////////////////////////////////////////////////////////////////////////////////
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "pfs/netty/default_archive_traits.hpp"

#if QT_ENABLED
#   include "pfs/netty/qt_archive_traits.hpp"
#endif

template <typename ArchiveType>
void tests ()
{
    using archive_traits = netty::archive_traits<ArchiveType>;
    char const * abc = "ABC";

    auto ar1 = archive_traits::make_archive(abc, 3);

    CHECK_EQ(ar1.size(), 3);
}

TEST_CASE("default") {
    tests<std::vector<char>>();

#if QT_ENABLED
    tests<QByteArray>();
#endif
}
