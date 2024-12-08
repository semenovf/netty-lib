################################################################################
# Copyright (c) 2021-2024 Vladislav Trifochkin
#
# This file is part of `netty-lib`.
#
# Changelog:
#       2021.10.05 Initial version.
#       2022.01.14 Refactored for using portable_target `ADD_TEST`.
#       2024.04.03 Added selection of proper dependencies.
#       2024.12.08 Removed `portable_target` dependency.
################################################################################
project(netty-lib-TESTS CXX C)

set(TESTS
    inet4_addr)

foreach (target ${TESTS})
    add_executable(${target} ${target}.cpp)
    target_link_libraries(${target} PRIVATE pfs::netty)
    add_test(NAME ${target} COMMAND ${target})
endforeach()
