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
    unreachable
    messaging)

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
    unreachable
    messaging
    delivery)

foreach (target ${TESTS})
    add_executable(tests-meshnet-${target}-reliable ${target}.cpp mesh_network.cpp)
    target_link_libraries(tests-meshnet-${target}-reliable PRIVATE pfs::netty pfs::lorem)
    target_compile_definitions(tests-meshnet-${target}-reliable PRIVATE "NETTY__TESTS_USE_MESHNET_RELIABLE_NODE=1")
    add_test(NAME tests-meshnet-${target}-reliable COMMAND tests-meshnet-${target}-reliable)

    if (Qt${QT_VERSION_MAJOR}_FOUND)
        add_executable(tests-meshnet-${target}-reliable-qt ${target}.cpp mesh_network.cpp)
        target_link_libraries(tests-meshnet-${target}-reliable-qt PRIVATE pfs::netty pfs::lorem Qt${QT_VERSION_MAJOR}::Core)
        target_compile_definitions(tests-meshnet-${target}-reliable-qt
            PRIVATE "NETTY__QT_ENABLED=1"
                    "NETTY__TESTS_USE_MESHNET_RELIABLE_NODE=1")
            add_test(NAME tests-meshnet-${target}-reliable-qt COMMAND tests-meshnet-${target}-reliable-qt)
    endif()
endforeach()
