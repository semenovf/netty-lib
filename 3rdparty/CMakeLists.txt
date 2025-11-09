################################################################################
# Copyright (c) 2025 Vladislav Trifochkin
#
# This file is part of `netty-lib`.
#
# Changelog:
#       2025.04.12 Initial version
################################################################################
include(FetchContent)

if (NETTY__ENABLE_ENET)
    if (NETTY__DISABLE_FETCH_CONTENT AND EXISTS ${CMAKE_CURRENT_LIST_DIR}/enet/.git)
        add_subdirectory(enet 3rdparty/enet)
    else()
        set(FETCHCONTENT_UPDATES_DISCONNECTED_ENET ON)
        message(STATUS "Fetching ENet ...")
        FetchContent_Declare(enet
            GIT_REPOSITORY https://github.com/lsalzman/enet.git
            GIT_TAG "v1.3.18"
            SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/enet)
            # SUBBUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/enet
        FetchContent_MakeAvailable(enet)
        message(STATUS "Fetching ENet complete")
    endif()

    target_include_directories(enet PUBLIC ${CMAKE_CURRENT_LIST_DIR}/enet/include)

    # This link can be removed with new ENet release (> 1.3.18)
    if (MSVC)
        cmake_policy(SET CMP0079 NEW)
        target_link_libraries(enet PRIVATE winmm ws2_32)
    endif()
endif(NETTY__ENABLE_ENET)

if (NETTY__BUILD_DEMO)
# TODO Remove below dependencies and dependent code in Demo
if (FALSE)
    # ZeroMQ options
    set(ENABLE_DRADTS OFF) # CACHE BOOL "" FORCE)
    set(ZMQ_BUILD_TESTS OFF) # CACHE BOOL "" FORCE)
    set(WITH_DOC OFF)
    set(WITH_DOCS OFF)

    if (NETTY__DISABLE_FETCH_CONTENT AND EXISTS ${CMAKE_CURRENT_LIST_DIR}/libzmq/.git)
        add_subdirectory(libzmq)
    else()
        set(FETCHCONTENT_UPDATES_DISCONNECTED_LIBZMQ ON)
        message(STATUS "Fetching libzmq ...")
        FetchContent_Declare(libzmq
            GIT_REPOSITORY https://github.com/zeromq/libzmq.git
            GIT_TAG "v4.3.5"
            GIT_SHALLOW 1
            SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/libzmq)
        FetchContent_MakeAvailable(libzmq)
        message(STATUS "Fetching libzmq complete")
    endif()

    set(CPPZMQ_BUILD_TESTS OFF) # CACHE BOOL "" FORCE)

    if (NETTY__DISABLE_FETCH_CONTENT AND EXISTS ${CMAKE_CURRENT_LIST_DIR}/cppzmq/.git)
        add_subdirectory(cppzmq)
    else()
        set(FETCHCONTENT_UPDATES_DISCONNECTED_CPPZMQ ON)
        message(STATUS "Fetching cppzmq ...")
        FetchContent_Declare(cppzmq
            GIT_REPOSITORY https://github.com/zeromq/cppzmq.git
            GIT_TAG "v4.11.0"
            GIT_SHALLOW 1
            SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/cppzmq)
        FetchContent_MakeAvailable(cppzmq)
        message(STATUS "Fetching cppzmq complete")
    endif()

    set(MSGPACK_USE_BOOST OFF CACHE BOOL "" FORCE)

    if (NETTY__DISABLE_FETCH_CONTENT AND EXISTS ${CMAKE_CURRENT_LIST_DIR}/msgpack-c/.git)
        add_subdirectory(msgpack-c)
    else()
        set(FETCHCONTENT_UPDATES_DISCONNECTED_MSGPACK ON)
        message(STATUS "Fetching msgpack-c ...")
        FetchContent_Declare(msgpack
            GIT_REPOSITORY https://github.com/msgpack/msgpack-c.git
            GIT_TAG "cpp-7.0.0"
            GIT_SHALLOW 1
            SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/msgpack-c)
        FetchContent_MakeAvailable(msgpack)
        message(STATUS "Fetching msgpack-c complete")
    endif()
endif()
endif(NETTY__BUILD_DEMO)
