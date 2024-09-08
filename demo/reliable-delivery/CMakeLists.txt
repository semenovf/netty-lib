################################################################################
# Copyright (c) 2019-2024 Vladislav Trifochkin
#
# This file is part of `netty-lib`.
#
# Changelog:
#      2024.05.01 Initial version.
################################################################################
project(reliable-delivery-demo)

portable_target(GET NETTY__SELECT_ENABLED _select_enabled OFF)
portable_target(GET NETTY__POLL_ENABLED _poll_enabled OFF)
portable_target(GET NETTY__EPOLL_ENABLED _epoll_enabled OFF)
portable_target(GET NETTY__ENET_ENABLED _enet_enabled OFF)
portable_target(GET NETTY__UDT_ENABLED _udt_enabled OFF)

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

foreach (_index RANGE 0 ${_length} 1)
    list(GET _suffixes ${_index} _suffix)
    list(GET _impls ${_index} _impl)

    set(_target ${PROJECT_NAME}-${_suffix})

    add_executable(${_target} main.cpp
        ${CMAKE_SOURCE_DIR}/src/sample/persistent_storage.cpp
        ${CMAKE_SOURCE_DIR}/src/sample/persistent_storage.hpp)

    target_compile_definitions(${_target} PRIVATE "${_impl}=1")
    target_include_directories(${_target} PRIVATE "${CMAKE_SOURCE_DIR}/src/sample")

    if (TARGET pfs::netty_p2p)
        target_link_libraries(${_target} pfs::netty_p2p)
    else()
        target_link_libraries(${_target} pfs::netty_p2p::static)
    endif()

    if (TARGET pfs::netty)
        target_link_libraries(${_target} pfs::netty)
    else()
        target_link_libraries(${_target} pfs::netty::static)
    endif()

    if (TARGET pfs::debby)
        target_link_libraries(${_target} pfs::debby)
    else()
        target_link_libraries(${_target} pfs::debby::static)
    endif()
endforeach()