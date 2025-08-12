################################################################################
# Copyright (c) 2025 Vladislav Trifochkin
#
# This file is part of `netty-lib`.
#
# Changelog:
#       2025.08.12 Initial version.
################################################################################
set(TESTS
    delivery
    handshake
    messaging
    priority_tracker
    protocol
    routing_table
    unreachable)

foreach (target ${TESTS})
    add_executable(tests-meshnet-${target} ${target}.cpp)
    target_link_libraries(tests-meshnet-${target} PRIVATE pfs::netty pfs::lorem)
    add_test(NAME tests-meshnet-${target} COMMAND tests-meshnet-${target})
endforeach()
