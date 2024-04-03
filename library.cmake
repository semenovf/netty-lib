################################################################################
# Copyright (c) 2021-2024 Vladislav Trifochkin
#
# This file is part of `netty-lib`.
#
# Changelog:
#      2021.06.21 Initial version.
#      2021.06.22 Fixed completely.
#      2022.01.14 Refactored for use `portable_target`.
#      2023.02.10 Separated static and shared builds.
#      2024.04.03 Replaced the sequence of two target configurations with a foreach statement.
################################################################################
cmake_minimum_required (VERSION 3.11)
project(netty CXX)

include(CheckIncludeFile)

option(NETTY__BUILD_SHARED "Enable build shared library" OFF)
option(NETTY__BUILD_STATIC "Enable build static library" ON)
option(NETTY__ENABLE_UDT "Enable modified UDT library (reliable UDP implementation)" OFF)
option(NETTY__UDT_PATCHED "Enable modified UDT library with patches" ON)
option(NETTY__ENABLE_QT5 "Enable Qt5 library (network backend)" OFF)

if (NOT PORTABLE_TARGET__CURRENT_PROJECT_DIR)
    set(PORTABLE_TARGET__CURRENT_PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
endif()

if (NETTY__BUILD_SHARED)
    portable_target(ADD_SHARED ${PROJECT_NAME} ALIAS pfs::netty EXPORTS NETTY__EXPORTS)
    list(APPEND _netty__targets ${PROJECT_NAME})
endif()

if (NETTY__BUILD_STATIC)
    set(STATIC_PROJECT_NAME ${PROJECT_NAME}-static)
    portable_target(ADD_STATIC ${STATIC_PROJECT_NAME} ALIAS pfs::netty::static EXPORTS NETTY__STATIC)
    list(APPEND _netty__targets ${STATIC_PROJECT_NAME})
endif()

if (PFS__LOG_LEVEL)
    list(APPEND _netty__definitions "PFS__LOG_LEVEL=${PFS__LOG_LEVEL}")
endif()

list(APPEND _netty__sources
    ${CMAKE_CURRENT_LIST_DIR}/src/error.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/inet4_addr.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/socket4_addr.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/startup.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/posix/inet_socket.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/posix/tcp_server.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/posix/tcp_socket.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/posix/udp_receiver.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/posix/udp_socket.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/posix/udp_sender.cpp)

if (UNIX OR ANDROID)
    list(APPEND _netty__sources
        ${CMAKE_CURRENT_LIST_DIR}/src/network_interface.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/network_interface_linux.cpp)
elseif (MSVC)
    list(APPEND _netty__sources
        ${CMAKE_CURRENT_LIST_DIR}/src/network_interface.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/network_interface_win32.cpp)
else()
    message (FATAL_ERROR "Unsupported platform")
endif()

if (NOT TARGET pfs::common)
    portable_target(INCLUDE_PROJECT ${CMAKE_CURRENT_LIST_DIR}/2ndparty/common/library.cmake)
endif()

if (MSVC)
    list(APPEND _netty__compile_options "/wd4251" "/wd4267" "/wd4244")
    list(APPEND _netty__private_links Ws2_32 Iphlpapi)
endif(MSVC)

CHECK_INCLUDE_FILE("poll.h" __has_poll)

if (__has_poll)
    list(APPEND _netty__sources
        ${CMAKE_CURRENT_LIST_DIR}/src/posix/connecting_poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/posix/listener_poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/posix/reader_poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/posix/poll_poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/posix/writer_poller.cpp)
    list(APPEND _netty__definitions "NETTY__POLL_ENABLED=1")
endif()

if (NOT MSVC)
    CHECK_INCLUDE_FILE("sys/select.h" __has_sys_select) # Linux
endif()

if (MSVC OR __has_sys_select)
    list(APPEND _netty__sources
        ${CMAKE_CURRENT_LIST_DIR}/src/posix/connecting_poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/posix/listener_poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/posix/reader_poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/posix/select_poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/posix/writer_poller.cpp)
    list(APPEND _netty__definitions "NETTY__SELECT_ENABLED=1")
endif()

if (UNIX OR ANDROID)
    CHECK_INCLUDE_FILE("sys/epoll.h" __has_sys_epoll)

    if (__has_sys_epoll)
        list(APPEND _netty__sources
            ${CMAKE_CURRENT_LIST_DIR}/src/linux/connecting_poller.cpp
            ${CMAKE_CURRENT_LIST_DIR}/src/linux/epoll_poller.cpp
            ${CMAKE_CURRENT_LIST_DIR}/src/linux/listener_poller.cpp
            ${CMAKE_CURRENT_LIST_DIR}/src/linux/reader_poller.cpp
            ${CMAKE_CURRENT_LIST_DIR}/src/linux/writer_poller.cpp)
        list(APPEND _netty__definitions "NETTY__EPOLL_ENABLED=1")
    endif()

    CHECK_INCLUDE_FILE("libmnl/libmnl.h" __has_libmnl)

    list(APPEND _netty__sources
        ${CMAKE_CURRENT_LIST_DIR}/src/linux/netlink_monitor.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/linux/netlink_socket.cpp)

    if (__has_libmnl)
        list(APPEND _netty__definitions "NETTY__LIBMNL_ENABLED=1")
        list(APPEND _netty__private_links mnl)
    endif()
endif()

if (NETTY__ENABLE_UDT)
    set(NETTY__UDT_ENABLED ON CACHE BOOL "UDT (modified) enabled")
    set(NETTY__UDT_ROOT "${CMAKE_CURRENT_LIST_DIR}/src/udt/newlib")

    # UDT sources
    list(APPEND _netty__sources
        ${NETTY__UDT_ROOT}/api.cpp
        ${NETTY__UDT_ROOT}/buffer.cpp
        ${NETTY__UDT_ROOT}/cache.cpp
        ${NETTY__UDT_ROOT}/ccc.cpp
        ${NETTY__UDT_ROOT}/channel.cpp
        ${NETTY__UDT_ROOT}/common.cpp
        ${NETTY__UDT_ROOT}/core.cpp
        ${NETTY__UDT_ROOT}/epoll.cpp
        ${NETTY__UDT_ROOT}/list.cpp
        ${NETTY__UDT_ROOT}/md5.cpp
        ${NETTY__UDT_ROOT}/packet.cpp
        ${NETTY__UDT_ROOT}/queue.cpp
        ${NETTY__UDT_ROOT}/window.cpp)

    list(APPEND _netty__sources
        ${CMAKE_CURRENT_LIST_DIR}/src/udt/connecting_poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/udt/epoll_poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/udt/listener_poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/udt/reader_poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/udt/udt_server.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/udt/udt_socket.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/udt/debug_CCC.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/udt/writer_poller.cpp)

    if (NETTY__BUILD_SHARED)
        portable_target(DEFINITIONS ${PROJECT_NAME} PUBLIC UDT_EXPORTS)
    endif()

    if (NETTY__BUILD_STATIC)
        portable_target(DEFINITIONS ${STATIC_PROJECT_NAME} PUBLIC UDT_EXPORTS)
    endif()

    list(APPEND _netty__definitions "NETTY__UDT_ENABLED=1")

    if (NETTY__UDT_PATCHED)
        list(APPEND _netty__private_definitions "NETTY__UDT_PATCHED=1")
    endif(NETTY__UDT_PATCHED)
endif(NETTY__ENABLE_UDT)

if (NETTY__ENABLE_QT5)
    list(APPEND _netty__sources
        ${CMAKE_CURRENT_LIST_DIR}/src/qt5/udp_recever.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/qt5/udp_sender.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/qt5/udp_socket.cpp)
    list(APPEND _netty__definitions "NETTY__QT5_ENABLED=1")
    list(APPEND _netty__qt5_components Core Network)
endif(NETTY__ENABLE_QT5)

foreach(_target IN LISTS _netty__targets)
    portable_target(SOURCES ${_target} ${_netty__sources})
    portable_target(INCLUDE_DIRS ${_target} PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
    portable_target(LINK ${_target} PUBLIC pfs::common)

    if (_netty__compile_options)
        portable_target(COMPILE_OPTIONS ${_target} PUBLIC ${_netty__compile_options})
    endif()

    if (_netty__private_definitions)
        portable_target(DEFINITIONS ${_target} PRIVATE ${_netty__private_definitions})
    endif()

    if (_netty__definitions)
        portable_target(DEFINITIONS ${_target} PUBLIC ${_netty__definitions})
    endif()

    if (_netty__private_links)
        portable_target(LINK ${_target} PRIVATE ${_netty__private_links})
    endif()

    if (_netty__qt5_components)
        portable_target(LINK_QT5_COMPONENTS ${_target} PRIVATE ${_netty__qt5_components})
    endif()
endforeach()

if (ANDROID)
    portable_target(BUILD_JAR pfs.netty
        SOURCES
            ${CMAKE_CURRENT_LIST_DIR}/src/android/com/koushikdutta/async/AsyncDatagramSocket.java
            ${CMAKE_CURRENT_LIST_DIR}/src/android/com/koushikdutta/async/AsyncNetworkSocket.java
            ${CMAKE_CURRENT_LIST_DIR}/src/android/com/koushikdutta/async/AsyncSemaphore.java
            ${CMAKE_CURRENT_LIST_DIR}/src/android/com/koushikdutta/async/AsyncServer.java
            ${CMAKE_CURRENT_LIST_DIR}/src/android/com/koushikdutta/async/AsyncServerSocket.java
            ${CMAKE_CURRENT_LIST_DIR}/src/android/com/koushikdutta/async/AsyncSocket.java
            ${CMAKE_CURRENT_LIST_DIR}/src/android/com/koushikdutta/async/ByteBufferList.java
            ${CMAKE_CURRENT_LIST_DIR}/src/android/com/koushikdutta/async/ChannelWrapper.java
            ${CMAKE_CURRENT_LIST_DIR}/src/android/com/koushikdutta/async/DataEmitter.java
            ${CMAKE_CURRENT_LIST_DIR}/src/android/com/koushikdutta/async/DatagramChannelWrapper.java
            ${CMAKE_CURRENT_LIST_DIR}/src/android/com/koushikdutta/async/DataSink.java
            ${CMAKE_CURRENT_LIST_DIR}/src/android/com/koushikdutta/async/HostnameResolutionException.java
            ${CMAKE_CURRENT_LIST_DIR}/src/android/com/koushikdutta/async/SelectorWrapper.java
            ${CMAKE_CURRENT_LIST_DIR}/src/android/com/koushikdutta/async/ServerSocketChannelWrapper.java
            ${CMAKE_CURRENT_LIST_DIR}/src/android/com/koushikdutta/async/SocketChannelWrapper.java
            ${CMAKE_CURRENT_LIST_DIR}/src/android/com/koushikdutta/async/ThreadQueue.java
            ${CMAKE_CURRENT_LIST_DIR}/src/android/com/koushikdutta/async/Util.java
            ${CMAKE_CURRENT_LIST_DIR}/src/android/com/koushikdutta/async/callback/CompletedCallback.java
            ${CMAKE_CURRENT_LIST_DIR}/src/android/com/koushikdutta/async/callback/ConnectCallback.java
            ${CMAKE_CURRENT_LIST_DIR}/src/android/com/koushikdutta/async/callback/DataCallback.java
            ${CMAKE_CURRENT_LIST_DIR}/src/android/com/koushikdutta/async/callback/ListenCallback.java
            ${CMAKE_CURRENT_LIST_DIR}/src/android/com/koushikdutta/async/callback/SocketCreateCallback.java
            ${CMAKE_CURRENT_LIST_DIR}/src/android/com/koushikdutta/async/callback/ValueFunction.java
            ${CMAKE_CURRENT_LIST_DIR}/src/android/com/koushikdutta/async/callback/WritableCallback.java
            ${CMAKE_CURRENT_LIST_DIR}/src/android/com/koushikdutta/async/future/Cancellable.java
            ${CMAKE_CURRENT_LIST_DIR}/src/android/com/koushikdutta/async/future/DependentCancellable.java
            ${CMAKE_CURRENT_LIST_DIR}/src/android/com/koushikdutta/async/future/DependentFuture.java
            ${CMAKE_CURRENT_LIST_DIR}/src/android/com/koushikdutta/async/future/DoneCallback.java
            ${CMAKE_CURRENT_LIST_DIR}/src/android/com/koushikdutta/async/future/FailCallback.java
            ${CMAKE_CURRENT_LIST_DIR}/src/android/com/koushikdutta/async/future/FailConvertCallback.java
            ${CMAKE_CURRENT_LIST_DIR}/src/android/com/koushikdutta/async/future/FailRecoverCallback.java
            ${CMAKE_CURRENT_LIST_DIR}/src/android/com/koushikdutta/async/future/FutureCallback.java
            ${CMAKE_CURRENT_LIST_DIR}/src/android/com/koushikdutta/async/future/Future.java
            ${CMAKE_CURRENT_LIST_DIR}/src/android/com/koushikdutta/async/future/SimpleCancellable.java
            ${CMAKE_CURRENT_LIST_DIR}/src/android/com/koushikdutta/async/future/SimpleFuture.java
            ${CMAKE_CURRENT_LIST_DIR}/src/android/com/koushikdutta/async/future/SuccessCallback.java
            ${CMAKE_CURRENT_LIST_DIR}/src/android/com/koushikdutta/async/future/ThenCallback.java
            ${CMAKE_CURRENT_LIST_DIR}/src/android/com/koushikdutta/async/future/ThenFutureCallback.java
            ${CMAKE_CURRENT_LIST_DIR}/src/android/com/koushikdutta/async/util/Allocator.java
            ${CMAKE_CURRENT_LIST_DIR}/src/android/com/koushikdutta/async/util/ArrayDeque.java
            ${CMAKE_CURRENT_LIST_DIR}/src/android/com/koushikdutta/async/util/Charsets.java
            ${CMAKE_CURRENT_LIST_DIR}/src/android/com/koushikdutta/async/util/Deque.java
            ${CMAKE_CURRENT_LIST_DIR}/src/android/com/koushikdutta/async/util/StreamUtility.java
            ${CMAKE_CURRENT_LIST_DIR}/src/android/com/koushikdutta/async/wrapper/AsyncSocketWrapper.java
            ${CMAKE_CURRENT_LIST_DIR}/src/android/com/koushikdutta/async/wrapper/DataEmitterWrapper.java

            ${CMAKE_CURRENT_LIST_DIR}/src/android/pfs/netty/AsyncRpcException.java
            ${CMAKE_CURRENT_LIST_DIR}/src/android/pfs/netty/AsyncRpcService.java
            ${CMAKE_CURRENT_LIST_DIR}/src/android/pfs/netty/ContentInfo.java
            ${CMAKE_CURRENT_LIST_DIR}/src/android/pfs/netty/ContentProviderBridge.java
            ${CMAKE_CURRENT_LIST_DIR}/src/android/pfs/netty/FileRpcRouter.java
            ${CMAKE_CURRENT_LIST_DIR}/src/android/pfs/netty/LogTag.java
            ${CMAKE_CURRENT_LIST_DIR}/src/android/pfs/netty/RpcRouter.java
            ${CMAKE_CURRENT_LIST_DIR}/src/android/pfs/netty/RpcService.java
        LINK_ANDROID)
endif()

