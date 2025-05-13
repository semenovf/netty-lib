################################################################################
# Copyright (c) 2021-2025 Vladislav Trifochkin
#
# This file is part of `netty-lib`.
#
# Changelog:
#       2021.10.05 Initial version.
#       2022.01.14 Refactored for using portable_target `ADD_TEST`.
#       2024.04.03 Added selection of proper dependencies.
#       2024.12.08 Removed `portable_target` dependency.
#       2024.12.25 Added `single_channel_connection` test.
#       2024.02.13 Added `reliable_delivery_engine` test.
################################################################################
project(netty-lib-TESTS CXX C)

####################################################################################################
# Common netty tests
####################################################################################################

set(TESTS inet4_addr)

foreach (target ${TESTS})
    add_executable(${target} ${target}.cpp)
    target_link_libraries(${target} PRIVATE pfs::netty pfs::lorem)
    add_test(NAME ${target} COMMAND ${target})
endforeach()

####################################################################################################
# Meshnet tests
####################################################################################################
add_executable(meshnet-duplication-tests meshnet/duplication.cpp)
target_link_libraries(meshnet-duplication-tests PRIVATE pfs::netty pfs::lorem)
add_test(NAME meshnet-duplication-tests COMMAND meshnet_duplication)

add_executable(meshnet-routing-table-tests meshnet/routing_table.cpp)
target_link_libraries(meshnet-routing-table-tests PRIVATE pfs::netty pfs::lorem)
add_test(NAME meshnet-routing-table-tests COMMAND meshnet_routing_table)

add_executable(meshnet-messaging-tests meshnet/messaging.cpp)
target_link_libraries(meshnet-messaging-tests PRIVATE pfs::netty pfs::lorem)
add_test(NAME meshnet-messaging-tests COMMAND meshnet-messaging-tests)

add_executable(meshnet-unreachable-tests meshnet/unreachable.cpp)
target_link_libraries(meshnet-unreachable-tests PRIVATE pfs::netty pfs::lorem)
add_test(NAME meshnet-unreachable-tests COMMAND meshnet-unreachable-tests)

add_executable(meshnet-delivery-tests meshnet/delivery.cpp)
target_link_libraries(meshnet-delivery-tests PRIVATE pfs::netty pfs::lorem)
add_test(NAME meshnet-delivery-tests COMMAND meshnet-delivery-tests)

####################################################################################################
# Channel connection tests
####################################################################################################
get_target_property(_select_enabled netty NETTY__SELECT_ENABLED)
get_target_property(_poll_enabled netty NETTY__POLL_ENABLED)
get_target_property(_epoll_enabled netty NETTY__EPOLL_ENABLED)
get_target_property(_enet_enabled netty NETTY__ENET_ENABLED)
get_target_property(_udt_enabled netty NETTY__UDT_ENABLED)

if (_select_enabled)
    add_executable(single_channel_connection_select single_channel_connection.cpp)
    target_link_libraries(single_channel_connection_select PRIVATE pfs::netty pfs::lorem)
    target_compile_definitions(single_channel_connection_select PRIVATE "NETTY__SCC_TEST_SELECT=1")
    add_test(NAME single_channel_connection_select COMMAND single_channel_connection_select)
endif()

if (_poll_enabled)
    add_executable(single_channel_connection_poll single_channel_connection.cpp)
    target_link_libraries(single_channel_connection_poll PRIVATE pfs::netty pfs::lorem)
    target_compile_definitions(single_channel_connection_poll PRIVATE "NETTY__SCC_TEST_POLL=1")
    add_test(NAME single_channel_connection_poll COMMAND single_channel_connection_poll)
endif()

if (_epoll_enabled)
    add_executable(single_channel_connection_epoll single_channel_connection.cpp)
    target_link_libraries(single_channel_connection_epoll PRIVATE pfs::netty pfs::lorem)
    target_compile_definitions(single_channel_connection_epoll PRIVATE "NETTY__SCC_TEST_POLL=1")
    add_test(NAME single_channel_connection_epoll COMMAND single_channel_connection_epoll)
endif()

if (_enet_enabled)
    add_executable(single_channel_connection_enet single_channel_connection.cpp)
    target_link_libraries(single_channel_connection_enet PRIVATE pfs::netty pfs::lorem)
    target_compile_definitions(single_channel_connection_enet PRIVATE "NETTY__SCC_TEST_ENET=1")
    add_test(NAME single_channel_connection_enet COMMAND single_channel_connection_enet)
endif()

if (_udt_enabled)
    add_executable(single_channel_connection_udt single_channel_connection.cpp)
    target_link_libraries(single_channel_connection_udt PRIVATE pfs::netty pfs::lorem)
    target_compile_definitions(single_channel_connection_udt PRIVATE "NETTY__SCC_TEST_UDT=1")
    add_test(NAME single_channel_connection_udt COMMAND single_channel_connection_udt)
endif()
