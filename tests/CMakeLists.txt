################################################################################
# Copyright (c) 2021 Vladislav Trifochkin
#
# This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
#
# Changelog:
#      2021.10.05 Initial version.
#      2022.01.14 Refactored for using portable_target `ADD_TEST`.
################################################################################
project(netty-lib-TESTS CXX C)

set(TESTS
    engine
    envelope
    packet_serializer)

foreach (t ${TESTS})
    portable_target(ADD_TEST ${t}
        ENABLE_COVERAGE ${NETTY__ENABLE_COVERAGE}
        SOURCES ${t}.cpp ${${t}_SOURCES}
        LINK pfs::netty::p2p)
endforeach()
