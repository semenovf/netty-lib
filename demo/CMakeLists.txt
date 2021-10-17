################################################################################
# Copyright (c) 2021 Vladislav Trifochkin
#
# This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
#
# Changelog:
#      2021.06.21 Initial version.
################################################################################
add_subdirectory(mtu)

# if (ENABLE_LIBZMQ)
#     add_subdirectory(lazypirate)
# endif(ENABLE_LIBZMQ)

if (QT5_NETWORK_ENABLED)
#     add_subdirectory(handshake)
#     add_subdirectory(channel)
#     add_subdirectory(p2p)
endif(QT5_NETWORK_ENABLED)
