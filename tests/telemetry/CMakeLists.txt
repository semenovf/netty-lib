################################################################################
# Copyright (c) 2025 Vladislav Trifochkin
#
# This file is part of `netty-lib`.
#
# Changelog:
#       2025.08.14 Initial version.
################################################################################
set(TESTS telemetry)

foreach (target ${TESTS})
    add_executable(tests-telemetry-${target} ${target}.cpp)
    target_link_libraries(tests-telemetry-${target} PRIVATE pfs::netty pfs::lorem)
    add_test(NAME tests-telemetry-${target} COMMAND tests-telemetry-${target})

    if (Qt${QT_VERSION_MAJOR}_FOUND)
        add_executable(tests-telemetry-${target}-qt ${target}.cpp)
        target_link_libraries(tests-telemetry-${target}-qt PRIVATE pfs::netty pfs::lorem Qt${QT_VERSION_MAJOR}::Core)
        target_compile_definitions(tests-telemetry-${target}-qt PRIVATE "NETTY__QT_ENABLED=1")
        add_test(NAME tests-telemetry-${target}-qt COMMAND tests-telemetry-${target}-qt)
    endif()
endforeach()
