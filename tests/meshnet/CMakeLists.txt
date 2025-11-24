################################################################################
# Copyright (c) 2025 Vladislav Trifochkin
#
# This file is part of `netty-lib`.
#
# Changelog:
#       2025.08.12 Initial version.
################################################################################
set(TESTS
    protocol
    priority_tracker
    priority_frame
    alive_controller
    input_controller
    handshake_controller
    heartbeat_controller
    priority_writer_queue
    handshake
    routing_table
    unreachable
    delivery
    # messaging
    )

foreach (target ${TESTS})
    add_executable(tests-meshnet-${target} ${target}.cpp)
    target_link_libraries(tests-meshnet-${target} PRIVATE pfs::netty pfs::lorem)
    add_test(NAME tests-meshnet-${target} COMMAND tests-meshnet-${target})

    if (Qt${QT_VERSION_MAJOR}_FOUND)
        add_executable(tests-meshnet-${target}-qt ${target}.cpp)
        target_link_libraries(tests-meshnet-${target}-qt PRIVATE pfs::netty pfs::lorem Qt${QT_VERSION_MAJOR}::Core)
        target_compile_definitions(tests-meshnet-${target}-qt PRIVATE "NETTY__QT_ENABLED=1")
        add_test(NAME tests-meshnet-${target}-qt COMMAND tests-meshnet-${target}-qt)
    endif()
endforeach()
