////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.11.22 Initial version.
////////////////////////////////////////////////////////////////////////////////
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "serializer_traits.hpp"
#include "pfs/netty/poller_types.hpp"
#include "pfs/netty/reader_pool.hpp"
#include "pfs/netty/posix/tcp_socket.hpp"

#if NETTY__EPOLL_ENABLED
using reader_poller_t = netty::reader_epoll_poller_t;
#elif NETTY__POLL_ENABLED
using reader_poller_t = netty::reader_poll_poller_t;
#elif NETTY__SELECT_ENABLED
using reader_poller_t = netty::reader_select_poller_t;
#endif

TEST_CASE("basic") {
    using reader_pool_t = netty::reader_pool<netty::posix::tcp_socket, reader_poller_t, archive_t>;

    reader_pool_t pool;
    pool.step();

    // TODO
}
