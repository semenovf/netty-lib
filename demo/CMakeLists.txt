################################################################################
# Copyright (c) 2019-2023 Vladislav Trifochkin
#
# This file is part of `netty-lib`.
#
# Changelog:
#      2021.06.21 Initial version.
#      2023.02.15 Added `iface-monitor`.
################################################################################
add_subdirectory(mtu)
add_subdirectory(netty)
add_subdirectory(udp)
add_subdirectory(iface-monitor)

if (NETTY__BUILD_P2P)
    add_subdirectory(discovery)
    add_subdirectory(p2p)
endif(NETTY__BUILD_P2P)
