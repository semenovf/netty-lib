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
#include "pfs/netty/startup.hpp"
#include "pfs/netty/writer_pool.hpp"
#include "pfs/netty/posix/tcp_socket.hpp"

#if NETTY__EPOLL_ENABLED
using writer_poller_t = netty::writer_epoll_poller_t;
#elif NETTY__POLL_ENABLED
using writer_poller_t = netty::writer_poll_poller_t;
#elif NETTY__SELECT_ENABLED
using writer_poller_t = netty::writer_select_poller_t;
#endif

class writer_queue
{
public:
    using archive_type = archive_t;

private:
    archive_type _frame;

public:
    archive_type acquire_frame (std::size_t frame_size)
    {
        return _frame;
    }

    void enqueue (int /*priority*/, char const * /*data*/, std::size_t /*len*/)
    {}

    void shift (std::size_t /*n*/)
    {}

public: // static
    static constexpr int priority_count () noexcept
    {
        return 1;
    }
};

TEST_CASE("basic") {
    netty::startup_guard netty_startup;
    using writer_pool_t = netty::writer_pool<netty::posix::tcp_socket, writer_poller_t, writer_queue>;

    writer_pool_t pool;
    pool.enqueue_broadcast("ABC", 3);
    pool.step();

    // TODO
}
