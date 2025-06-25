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
#      2024.12.08 Removed `portable_target` dependency.
#                 Min CMake version is 3.19.
################################################################################
cmake_minimum_required (VERSION 3.19)
project(netty CXX)

include(CheckIncludeFile)

if (NETTY__BUILD_SHARED)
    add_library(netty SHARED)
    target_compile_definitions(netty PUBLIC NETTY__EXPORTS)
else()
    add_library(netty STATIC)
    target_compile_definitions(netty PUBLIC NETTY__STATIC)
endif()

add_library(pfs::netty ALIAS netty)

if (NETTY__ENABLE_TRACE)
    target_compile_definitions(netty PUBLIC "NETTY__TRACE_ENABLED=1")
endif()

if (MSVC)
    target_compile_definitions(netty PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

target_sources(netty PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src/inet4_addr.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/socket4_addr.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/startup.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/posix/inet_socket.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/posix/tcp_listener.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/posix/tcp_socket.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/posix/udp_receiver.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/posix/udp_socket.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/posix/udp_sender.cpp)

if (NETTY__ENABLE_UTILS)
    if (UNIX OR ANDROID)
        target_sources(netty PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}/src/utils/network_interface.cpp
            ${CMAKE_CURRENT_LIST_DIR}/src/utils/network_interface_linux.cpp
            ${CMAKE_CURRENT_LIST_DIR}/src/utils/netlink_monitor_linux.cpp
            ${CMAKE_CURRENT_LIST_DIR}/src/utils/netlink_socket.cpp)
    elseif (MSVC)
        target_sources(netty PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}/src/utils/network_interface.cpp
            ${CMAKE_CURRENT_LIST_DIR}/src/utils/network_interface_win32.cpp
            ${CMAKE_CURRENT_LIST_DIR}/src/utils/netlink_monitor_win32.cpp)
    else()
        message (FATAL_ERROR "Unsupported platform")
    endif()
endif()

target_link_libraries(netty PUBLIC pfs::ionik)

if (MSVC)
    target_compile_options(netty PRIVATE "/wd4251" "/wd4267" "/wd4244")
    target_link_libraries(netty PRIVATE Ws2_32 Iphlpapi)
endif(MSVC)

check_include_file("poll.h" __has_poll)

if (__has_poll)
    target_sources(netty PRIVATE
#        ${CMAKE_CURRENT_LIST_DIR}/src/posix/client_poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/posix/connecting_poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/posix/listener_poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/posix/reader_poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/posix/poll_poller.cpp
#        ${CMAKE_CURRENT_LIST_DIR}/src/posix/server_poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/posix/writer_poller.cpp)
    target_compile_definitions(netty PUBLIC "NETTY__POLL_ENABLED=1")
    set_target_properties(netty PROPERTIES NETTY__POLL_ENABLED ON)
endif()

check_include_file("sys/select.h" __has_sys_select) # Linux

if (MSVC OR __has_sys_select)
    target_sources(netty PRIVATE
#        ${CMAKE_CURRENT_LIST_DIR}/src/posix/client_poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/posix/connecting_poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/posix/listener_poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/posix/reader_poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/posix/select_poller.cpp
#        ${CMAKE_CURRENT_LIST_DIR}/src/posix/server_poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/posix/writer_poller.cpp)
    target_compile_definitions(netty PUBLIC "NETTY__SELECT_ENABLED=1")
    set_target_properties(netty PROPERTIES NETTY__SELECT_ENABLED ON)
endif()

if (UNIX OR ANDROID)
    check_include_file("sys/epoll.h" __has_sys_epoll)

    if (__has_sys_epoll)
        target_sources(netty PRIVATE
#            ${CMAKE_CURRENT_LIST_DIR}/src/linux/client_poller.cpp
            ${CMAKE_CURRENT_LIST_DIR}/src/linux/connecting_poller.cpp
            ${CMAKE_CURRENT_LIST_DIR}/src/linux/epoll_poller.cpp
            ${CMAKE_CURRENT_LIST_DIR}/src/linux/listener_poller.cpp
            ${CMAKE_CURRENT_LIST_DIR}/src/linux/reader_poller.cpp
#            ${CMAKE_CURRENT_LIST_DIR}/src/linux/server_poller.cpp
            ${CMAKE_CURRENT_LIST_DIR}/src/linux/writer_poller.cpp)
        target_compile_definitions(netty PUBLIC "NETTY__EPOLL_ENABLED=1")
        set_target_properties(netty PROPERTIES NETTY__EPOLL_ENABLED ON)
    endif()

    check_include_file("libmnl/libmnl.h" __has_libmnl)

    if (__has_libmnl)
        target_compile_definitions(netty PUBLIC "NETTY__LIBMNL_ENABLED=1")
        target_link_libraries(netty PRIVATE mnl)
    endif()
endif()

if (NETTY__ENABLE_UDT)
    set(NETTY__UDT_ROOT "${CMAKE_CURRENT_LIST_DIR}/src/udt/newlib")

    # UDT sources
    target_sources(netty PRIVATE
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

    target_sources(netty PRIVATE
#        ${CMAKE_CURRENT_LIST_DIR}/src/udt/client_poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/udt/connecting_poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/udt/epoll_poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/udt/listener_poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/udt/reader_poller.cpp
#        ${CMAKE_CURRENT_LIST_DIR}/src/udt/server_poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/udt/udt_listener.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/udt/udt_socket.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/udt/debug_CCC.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/udt/writer_poller.cpp)

    target_compile_definitions(netty PRIVATE UDT_STATIC)
    target_compile_definitions(netty PUBLIC "NETTY__UDT_ENABLED=1")
    set_target_properties(netty PROPERTIES NETTY__UDT_ENABLED ON)

    if (NETTY__UDT_PATCHED)
        target_compile_definitions(netty PRIVATE "NETTY__UDT_PATCHED=1")
    endif(NETTY__UDT_PATCHED)
endif(NETTY__ENABLE_UDT)

if (NETTY__ENABLE_ENET)
    target_sources(netty PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/src/enet/enet_poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/enet/enet_listener.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/enet/enet_socket.cpp
        # ${CMAKE_CURRENT_LIST_DIR}/src/enet/client_poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/enet/connecting_poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/enet/listener_poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/enet/reader_poller.cpp
        # ${CMAKE_CURRENT_LIST_DIR}/src/enet/server_poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/enet/writer_poller.cpp)
    target_compile_definitions(netty PUBLIC "NETTY__ENET_ENABLED=1")
    target_link_libraries(netty PRIVATE enet)
    set_target_properties(netty PROPERTIES NETTY__ENET_ENABLED ON)
endif(NETTY__ENABLE_ENET)

if (NETTY__ENABLE_P2P)
    target_sources(netty PRIVATE
        # ${CMAKE_CURRENT_LIST_DIR}/src/p2p/remote_file_protocol.cpp
        # ${CMAKE_CURRENT_LIST_DIR}/src/p2p/remote_file_provider.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/p2p/posix/discovery_engine.cpp)

    target_link_libraries(netty PRIVATE pfs::ionik)
endif()

target_include_directories(netty
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/include/pfs
        ${CMAKE_CURRENT_LIST_DIR}/include/pfs/netty
        ${CMAKE_CURRENT_LIST_DIR}/include/pfs/netty/utils)

target_link_libraries(netty PUBLIC pfs::common)

if (ANDROID)
    find_package(Java "1.8" COMPONENTS Development REQUIRED)

    if (NOT DEFINED Java_Development_FOUND)
        message(FATAL_ERROR "No Java found, may be JAVA_HOME environment variable has invalid value")
    endif()

    include(UseJava)

    #set(CMAKE_JAVA_COMPILE_FLAGS ...)
    set(CMAKE_JAVA_INCLUDE_PATH "${CMAKE_ANDROID_NDK}/../platforms/${ANDROID_PLATFORM}/android.jar:${CMAKE_JAVA_INCLUDE_PATH}") # CLASSPATH

    add_jar(pfs.netty
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
            ${CMAKE_CURRENT_LIST_DIR}/src/android/pfs/netty/RpcService.java)

    get_target_property(_jar_file pfs.netty JAR_FILE)
    get_target_property(_class_dir pfs.netty CLASSDIR)

    message(TRACE "JAR file path: ${_jar_file}")
    message(TRACE "Class compiled to: ${_class_dir}")
endif()
