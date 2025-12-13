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
    messaging)

foreach (target ${TESTS})
    add_executable(tests-meshnet-legacy-${target} ${target}.cpp)
    target_link_libraries(tests-meshnet-legacy-${target} PRIVATE pfs::netty pfs::lorem)
    add_test(NAME tests-meshnet-legacy-${target} COMMAND tests-meshnet-legacy-${target})

    if (Qt${QT_VERSION_MAJOR}_FOUND)
        add_executable(tests-meshnet-legacy-${target}-qt ${target}.cpp)
        target_link_libraries(tests-meshnet-legacy-${target}-qt PRIVATE pfs::netty pfs::lorem Qt${QT_VERSION_MAJOR}::Core)
        target_compile_definitions(tests-meshnet-legacy-${target}-qt PRIVATE "NETTY__QT_ENABLED=1")
        add_test(NAME tests-meshnet-legacy-${target}-qt COMMAND tests-meshnet-legacy-${target}-qt)
    endif()
endforeach()
