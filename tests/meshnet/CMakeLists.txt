################################################################################
# Copyright (c) 2025 Vladislav Trifochkin
#
# This file is part of `netty-lib`.
#
# Changelog:
#       2025.12.08 Initial version.
################################################################################
set(TESTS
    protocol
    priority_tracker
    priority_frame
    alive_controller
    input_controller
    handshake_controller
    heartbeat_controller
    priority_writer_queue)

foreach (target ${TESTS})
    add_executable(tests-meshnet-${target} ${target}.cpp mesh_network.cpp)
    target_link_libraries(tests-meshnet-${target} PRIVATE pfs::netty pfs::lorem)
    add_test(NAME tests-meshnet-${target} COMMAND tests-meshnet-${target})

    if (Qt${QT_VERSION_MAJOR}_FOUND)
        add_executable(tests-meshnet-${target}-qt ${target}.cpp mesh_network.cpp)
        target_link_libraries(tests-meshnet-${target}-qt PRIVATE pfs::netty pfs::lorem Qt${QT_VERSION_MAJOR}::Core)
        target_compile_definitions(tests-meshnet-${target}-qt PRIVATE "NETTY__QT_ENABLED=1")
        add_test(NAME tests-meshnet-${target}-qt COMMAND tests-meshnet-${target}-qt)
    endif()
endforeach()

set(TESTS
    channel
    duplication
    routing
    alive)

foreach (target ${TESTS})
    add_executable(tests-meshnet-${target} ${target}.cpp mesh_network.cpp)
    target_link_libraries(tests-meshnet-${target} PRIVATE pfs::netty pfs::lorem)
    add_test(NAME tests-meshnet-${target} COMMAND tests-meshnet-${target})

    if (TRUE)
        add_executable(tests-meshnet-${target}-rd ${target}.cpp mesh_network.cpp)
        target_link_libraries(tests-meshnet-${target}-rd PRIVATE pfs::netty pfs::lorem)
        target_compile_definitions(tests-meshnet-${target}-rd PRIVATE "NETTY__TESTS_USE_MESHNET_NODE_POOL_RD=1")
        add_test(NAME tests-meshnet-${target}-rd COMMAND tests-meshnet-${target}-rd)

        if (Qt${QT_VERSION_MAJOR}_FOUND)
            add_executable(tests-meshnet-${target}-qt ${target}.cpp mesh_network.cpp)
            target_link_libraries(tests-meshnet-${target}-qt PRIVATE pfs::netty pfs::lorem Qt${QT_VERSION_MAJOR}::Core)
            target_compile_definitions(tests-meshnet-${target}-qt PRIVATE "NETTY__QT_ENABLED=1")
            add_test(NAME tests-meshnet-${target}-qt COMMAND tests-meshnet-${target}-qt)

            add_executable(tests-meshnet-${target}-rd-qt ${target}.cpp mesh_network.cpp)
            target_link_libraries(tests-meshnet-${target}-rd-qt PRIVATE pfs::netty pfs::lorem Qt${QT_VERSION_MAJOR}::Core)
            target_compile_definitions(tests-meshnet-${target}-rd-qt
                PRIVATE "NETTY__QT_ENABLED=1"
                        "NETTY__TESTS_USE_MESHNET_NODE_POOL_RD=1")
            add_test(NAME tests-meshnet-${target}-rd-qt COMMAND tests-meshnet-${target}-rd-qt)
        endif()
    endif()
endforeach()
