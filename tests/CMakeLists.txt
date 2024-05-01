################################################################################
# Copyright (c) 2021-2024 Vladislav Trifochkin
#
# This file is part of `netty-lib`.
#
# Changelog:
#      2021.10.05 Initial version.
#      2022.01.14 Refactored for using portable_target `ADD_TEST`.
#      2024.04.03 Added selection of proper dependencies.
################################################################################
project(netty-lib-TESTS CXX C)

set(TESTS
    inet4_addr)

if (TARGET pfs::netty)
    set(_netty__lib pfs::netty)
elseif(TARGET pfs::netty::static)
    set(_netty__lib pfs::netty::static)
endif()

if (TARGET pfs::netty_p2p)
    set(_netty_p2p__lib pfs::netty_p2p)
elseif(TARGET pfs::netty_p2p::static)
    set(_netty_p2p__lib pfs::netty_p2p::static)
endif()

foreach (t ${TESTS})
    portable_target(ADD_TEST ${t}
        ENABLE_COVERAGE ${NETTY__ENABLE_COVERAGE}
        SOURCES ${t}.cpp ${${t}_SOURCES}
        LINK ${_netty_p2p__lib} ${_netty__lib})
endforeach()
