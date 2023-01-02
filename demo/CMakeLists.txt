################################################################################
# Copyright (c) 2019,2022 Vladislav Trifochkin
#
# This file is part of `netty-lib`.
#
# Changelog:
#      2021.06.21 Initial version.
################################################################################
add_subdirectory(mtu)

# if (ENABLE_LIBZMQ)
#     add_subdirectory(lazypirate)
# endif(ENABLE_LIBZMQ)

add_subdirectory(posix_sockets)

if (NETTY__BUILD_P2P)
    #add_subdirectory(p2p) #  FIXME
endif(NETTY__BUILD_P2P)
