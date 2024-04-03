################################################################################
# Copyright (c) 2022-2024 Vladislav Trifochkin
#
# This file is part of `netty-lib`.
#
# Changelog:
#      2022.02.24 Initial version.
#      2024.04.03 Refactored.
################################################################################
set(SKIP_PORTABILITY_TEST ON CACHE BOOL "")
set(THREAD_SAFE OFF CACHE BOOL "")
set(SKIP_PERFORMANCE_COMPARISON ON CACHE BOOL "")
set(BUILD_DOC OFF CACHE BOOL "")
set(BUILD_SANDBOX OFF CACHE BOOL "")

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/cereal 3rdparty/cereal)
