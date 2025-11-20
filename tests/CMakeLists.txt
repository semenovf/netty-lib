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
#       2025.08.06 Added `envelope` test.
#       2025.08.06 Added `chunk` test.
#       2025.11.17 `chunk` renamed to `buffer`.
################################################################################
set(TESTS
    archive
    # envelope # FIXME FIX AND UNCOMMENT
    inet4_addr)

set(TESTS_QT archive) #buffer envelope)  # FIXME FIX AND UNCOMMENT

if (NOT QT_VERSION_MAJOR)
    if (Qt6_VERSION_MAJOR)
        set(QT_VERSION_MAJOR 6)
    elseif (Qt5_VERSION_MAJOR)
        set(QT_VERSION_MAJOR 5)
    else()
        find_package(QT NAMES Qt6 Qt5)
    endif()
endif()

if (QT_VERSION_MAJOR)
    find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core)
endif()

foreach (target ${TESTS})
    add_executable(tests-${target} ${target}.cpp)
    target_link_libraries(tests-${target} PRIVATE pfs::netty pfs::lorem)
    add_test(NAME tests-${target} COMMAND tests-${target})

    if (Qt${QT_VERSION_MAJOR}_FOUND)
        if (target IN_LIST TESTS_QT)
            add_executable(tests-${target}-qt ${target}.cpp)
            target_compile_definitions(tests-${target}-qt PRIVATE "NETTY__QT_ENABLED=1")
            target_link_libraries(tests-${target}-qt PRIVATE pfs::netty pfs::lorem Qt${QT_VERSION_MAJOR}::Core)
            add_test(NAME tests-${target}-qt COMMAND tests-${target}-qt)
        endif()
    endif()
endforeach()

add_subdirectory(meshnet)
#add_subdirectory(pubsub) # FIXME FIX AND UNCOMMENT
#add_subdirectory(telemetry) # FIXME FIX AND UNCOMMENT

####################################################################################################
# Channel connection tests
####################################################################################################
get_target_property(_select_enabled netty NETTY__SELECT_ENABLED)
get_target_property(_poll_enabled netty NETTY__POLL_ENABLED)
get_target_property(_epoll_enabled netty NETTY__EPOLL_ENABLED)
get_target_property(_enet_enabled netty NETTY__ENET_ENABLED)
get_target_property(_udt_enabled netty NETTY__UDT_ENABLED)

# FIXME
# if (_select_enabled)
#     add_executable(single_channel_connection_select single_channel_connection.cpp)
#     target_link_libraries(single_channel_connection_select PRIVATE pfs::netty pfs::lorem)
#     target_compile_definitions(single_channel_connection_select PRIVATE "NETTY__SCC_TEST_SELECT=1")
#     add_test(NAME single_channel_connection_select COMMAND single_channel_connection_select)
# endif()
#
# if (_poll_enabled)
#     add_executable(single_channel_connection_poll single_channel_connection.cpp)
#     target_link_libraries(single_channel_connection_poll PRIVATE pfs::netty pfs::lorem)
#     target_compile_definitions(single_channel_connection_poll PRIVATE "NETTY__SCC_TEST_POLL=1")
#     add_test(NAME single_channel_connection_poll COMMAND single_channel_connection_poll)
# endif()
#
# if (_epoll_enabled)
#     add_executable(single_channel_connection_epoll single_channel_connection.cpp)
#     target_link_libraries(single_channel_connection_epoll PRIVATE pfs::netty pfs::lorem)
#     target_compile_definitions(single_channel_connection_epoll PRIVATE "NETTY__SCC_TEST_POLL=1")
#     add_test(NAME single_channel_connection_epoll COMMAND single_channel_connection_epoll)
# endif()
#
# if (_enet_enabled)
#     add_executable(single_channel_connection_enet single_channel_connection.cpp)
#     target_link_libraries(single_channel_connection_enet PRIVATE pfs::netty pfs::lorem)
#     target_compile_definitions(single_channel_connection_enet PRIVATE "NETTY__SCC_TEST_ENET=1")
#     add_test(NAME single_channel_connection_enet COMMAND single_channel_connection_enet)
# endif()
#
# if (_udt_enabled)
#     add_executable(single_channel_connection_udt single_channel_connection.cpp)
#     target_link_libraries(single_channel_connection_udt PRIVATE pfs::netty pfs::lorem)
#     target_compile_definitions(single_channel_connection_udt PRIVATE "NETTY__SCC_TEST_UDT=1")
#     add_test(NAME single_channel_connection_udt COMMAND single_channel_connection_udt)
# endif()
