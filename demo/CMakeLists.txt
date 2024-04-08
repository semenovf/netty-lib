################################################################################
# Copyright (c) 2019-2024 Vladislav Trifochkin
#
# This file is part of `netty-lib`.
#
# Changelog:
#      2021.06.21 Initial version.
#      2023.02.15 Added `iface-monitor`.
#      2024.04.08 `available_net_interfaces` moved from `ionik-lib`.
################################################################################
add_subdirectory(available_net_interfaces)
add_subdirectory(mtu)
add_subdirectory(netty)
add_subdirectory(udp)
add_subdirectory(iface-monitor)
add_subdirectory(rpc)

if (NETTY__BUILD_P2P)
    add_subdirectory(discovery)
    add_subdirectory(p2p)
endif(NETTY__BUILD_P2P)
