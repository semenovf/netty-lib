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
endforeach()
