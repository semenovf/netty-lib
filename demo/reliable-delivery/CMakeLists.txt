################################################################################
# Copyright (c) 2019-2024 Vladislav Trifochkin
#
# This file is part of `netty-lib`.
#
# Changelog:
#       2024.05.01 Initial version.
#       2024.12.08 Removed `portable_target` dependency.
################################################################################
project(reliable-delivery-demo)

get_target_property(_select_enabled netty NETTY__SELECT_ENABLED)
get_target_property(_poll_enabled netty NETTY__POLL_ENABLED)
get_target_property(_epoll_enabled netty NETTY__EPOLL_ENABLED)
get_target_property(_enet_enabled netty NETTY__ENET_ENABLED)
get_target_property(_udt_enabled netty NETTY__UDT_ENABLED)

if (_select_enabled)
    list(APPEND _suffixes select)
    list(APPEND _impls "NETTY__SELECT_IMPL")
endif()

if (_poll_enabled)
    list(APPEND _suffixes poll)
    list(APPEND _impls "NETTY__POLL_IMPL")
endif()

if (_epoll_enabled)
    list(APPEND _suffixes epoll)
    list(APPEND _impls "NETTY__EPOLL_IMPL")
endif()

if (_enet_enabled)
    list(APPEND _suffixes enet)
    list(APPEND _impls "NETTY__ENET_IMPL")
endif()

if (_udt_enabled)
    list(APPEND _suffixes udt)
    list(APPEND _impls "NETTY__UDT_IMPL")
endif()

list(LENGTH _suffixes _length)

math(EXPR _length "${_length}-1")

set(FETCHCONTENT_UPDATES_DISCONNECTED_DEBBY ON)
message(STATUS "Fetching pfs::debby ...")
include(FetchContent)
FetchContent_Declare(debby
    GIT_REPOSITORY https://github.com/semenovf/debby-lib.git
    GIT_TAG master
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/2ndparty/debby
    SUBBUILD_DIR ${CMAKE_BINARY_DIR}/2ndparty/debby)
FetchContent_MakeAvailable(debby)
message(STATUS "Fetching pfs::debby complete")

foreach (_index RANGE 0 ${_length} 1)
    list(GET _suffixes ${_index} _suffix)
    list(GET _impls ${_index} _impl)

    set(_target ${PROJECT_NAME}-${_suffix})

    add_executable(${_target} main.cpp
        ${CMAKE_SOURCE_DIR}/src/sample/persistent_storage.cpp
        ${CMAKE_SOURCE_DIR}/src/sample/persistent_storage.hpp)

    target_compile_definitions(${_target} PRIVATE "${_impl}=1")
    target_include_directories(${_target} PRIVATE "${CMAKE_SOURCE_DIR}/src/sample")
    target_link_libraries(${_target} PRIVATE pfs::netty pfs::debby)
endforeach()
