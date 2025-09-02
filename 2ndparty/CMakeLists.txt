################################################################################
# Copyright (c) 2025 Vladislav Trifochkin
#
# This file is part of `netty-lib`.
#
# Changelog:
#       2025.04.12 Initial version
#       2025.07.29 Using foreach() loop for fetching content.
################################################################################
include(FetchContent)

list(APPEND _deps common ionik)

if (NETTY__BUILD_TESTS)
    list(APPEND _deps lorem)
endif()

foreach (_dep ${_deps})
    if (NOT TARGET ${_dep})
        if (NETTY__DISABLE_FETCH_CONTENT AND EXISTS ${CMAKE_CURRENT_LIST_DIR}/${_dep}/.git)
            add_subdirectory(${_dep})
        else()
            message(STATUS "Fetching ${_dep} ...")
            FetchContent_Declare(${_dep}
                GIT_REPOSITORY https://github.com/semenovf/${_dep}-lib.git
                GIT_TAG master
                SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/${_dep})
            FetchContent_MakeAvailable(${_dep})
            message(STATUS "Fetching ${_dep} complete")
        endif()
    endif()
endforeach()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_LIST_DIR}/common/cmake" PARENT_SCOPE)
