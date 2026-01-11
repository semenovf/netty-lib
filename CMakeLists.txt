################################################################################
# Copyright (c) 2021-2025 Vladislav Trifochkin
#
# This file is part of `netty-lib`.
#
# Changelog:
#       2021.06.21 Initial version
#       2022.01.14 Updated according new template.
#       2024.04.03 Fixed `portable-target` path.
#       2024.12.07 Up to C++14 standard.
#                  Min CMake version is 3.19 (CMakePresets).
#       2025.11.09 Merged with library.cmake.
################################################################################
cmake_minimum_required (VERSION 3.19)
project(netty CXX C)

include(CheckIncludeFile)

option(NETTY__BUILD_STRICT "Build with strict policies: C++ standard required, C++ extension is OFF etc" ON)
option(NETTY__BUILD_TESTS "Build tests" OFF)
option(NETTY__BUILD_DEMO "Build examples/demo" OFF)

# Netty library specific options
option(NETTY__BUILD_STATIC "Force build static library" OFF)
option(NETTY__ENABLE_P2P "Enabled P2P support" OFF) # FIXME DEPRECATED
option(NETTY__ENABLE_UDT "Enable modified UDT library (reliable UDP implementation)" OFF)
option(NETTY__UDT_PATCHED "Enable modified UDT library with patches" ON)
option(NETTY__ENABLE_ENET "Enable ENet library (reliable UDP implementation)" OFF)
option(NETTY__ENABLE_UTILS "Enable utility classes: netlink monitor, enumerate network interfaces etc" ON)
option(NETTY__ENABLE_TELEMETRY "Enable telemetry for meshnet and delivery patterns" OFF)
option(NETTY__ENABLE_AGGRESSIVE_COMPILE_CHECK "Use aggressive check compile options (g++ only)" OFF)
option(NETTY__ENABLE_TRACE "Enable trace messages output" OFF)
option(NETTY__DISABLE_FETCH_CONTENT "Disable fetch content if sources of dependencies already exists in the working tree (checks .git subdirectory)" ON)
option(NETTY__MESHNET_SERIAL_FIELD_SUPPORT "Add serial field to priority frame (used for debugging meshnet pattern generally)" OFF)

if (NETTY__BUILD_STRICT)
    if (NOT CMAKE_CXX_STANDARD)
        set(CMAKE_CXX_STANDARD 14)
    endif()

    set(CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF) # use -std=c++XX rather than -std=gnu++XX
endif()

add_subdirectory(3rdparty)
add_subdirectory(2ndparty)

####################################################################################################
# library specific block
####################################################################################################
if (BUILD_SHARED_LIBS AND NOT NETTY__BUILD_STATIC)
    add_library(netty)
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

check_include_file("netdb.h" __has_netdb_h)

if (__has_netdb_h)
    target_sources(netty PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src/posix/inet4_addr.cpp)
endif()

check_include_file("poll.h" __has_poll)

if (__has_poll)
    target_sources(netty PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/src/posix/connecting_poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/posix/listener_poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/posix/reader_poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/posix/poll_poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/posix/writer_poller.cpp)
    target_compile_definitions(netty PUBLIC "NETTY__POLL_ENABLED=1")
    set_target_properties(netty PROPERTIES NETTY__POLL_ENABLED ON)
endif()

check_include_file("sys/select.h" __has_sys_select) # Linux

if (MSVC OR __has_sys_select)
    target_sources(netty PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/src/posix/connecting_poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/posix/listener_poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/posix/reader_poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/posix/select_poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/posix/writer_poller.cpp)
    target_compile_definitions(netty PUBLIC "NETTY__SELECT_ENABLED=1")
    set_target_properties(netty PROPERTIES NETTY__SELECT_ENABLED ON)
endif()

if (UNIX OR ANDROID)
    check_include_file("sys/epoll.h" __has_sys_epoll)

    if (__has_sys_epoll)
        target_sources(netty PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}/src/linux/connecting_poller.cpp
            ${CMAKE_CURRENT_LIST_DIR}/src/linux/epoll_poller.cpp
            ${CMAKE_CURRENT_LIST_DIR}/src/linux/listener_poller.cpp
            ${CMAKE_CURRENT_LIST_DIR}/src/linux/reader_poller.cpp
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
        ${CMAKE_CURRENT_LIST_DIR}/src/udt/connecting_poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/udt/epoll_poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/udt/listener_poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/udt/reader_poller.cpp
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
        ${CMAKE_CURRENT_LIST_DIR}/src/enet/connecting_poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/enet/listener_poller.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/enet/reader_poller.cpp
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

endif()

if (NETTY__ENABLE_TELEMETRY)
    target_link_libraries(netty PRIVATE pfs::ionik)
    target_compile_definitions(netty PUBLIC "NETTY__TELEMETRY_ENABLED=1")
endif()

if (NETTY__MESHNET_SERIAL_FIELD_SUPPORT)
    target_compile_definitions(netty PUBLIC "NETTY__MESHNET_SERIAL_FIELD_SUPPORT=1")
endif()

target_include_directories(netty
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/include/pfs
        ${CMAKE_CURRENT_LIST_DIR}/include/pfs/netty
        ${CMAKE_CURRENT_LIST_DIR}/include/pfs/netty/utils)

target_link_libraries(netty PUBLIC pfs::common)

if (NETTY__ENABLE_AGGRESSIVE_COMPILE_CHECK)
    include(AggressiveCheckOpts)
    set(_is_sanitize_thread FALSE)
    aggressive_check_opts(netty ${_is_sanitize_thread})
endif()
####################################################################################################

if (NETTY__BUILD_TESTS AND EXISTS ${CMAKE_CURRENT_LIST_DIR}/tests)
    enable_testing()
    add_subdirectory(tests)
endif()

if (NETTY__BUILD_DEMO AND EXISTS ${CMAKE_CURRENT_LIST_DIR}/demo)
    add_subdirectory(demo)
endif()

include(GNUInstallDirs)

install(TARGETS netty
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
