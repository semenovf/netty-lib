////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.03 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <pfs/netty/error.hpp>
#include <pfs/netty/namespace.hpp>
#include <chrono>
#include <limits>
#include <vector>

#if _MSC_VER
#   ifdef FD_SETSIZE
#       undef FD_SETSIZE
#   endif
#   define FD_SETSIZE 512
#   include <winsock2.h>
#else
#   include <sys/select.h>
#endif

NETTY__NAMESPACE_BEGIN

namespace posix {

class select_poller
{
public:
#if _MSC_VER
    using socket_id = SOCKET;
    static socket_id const kINVALID_SOCKET = INVALID_SOCKET;
#else
    using socket_id = int;
    static socket_id constexpr kINVALID_SOCKET = -1;

    socket_id max_fd {0};
#endif

    using listener_id = socket_id;

    std::vector<socket_id> sockets;
    int count = 0;

    fd_set readfds;
    fd_set writefds;

    bool observe_read {false};
    bool observe_write {false};

public:
    select_poller (bool observe_read, bool observe_write);
    ~select_poller ();

    void add_socket (socket_id sock, error * perr = nullptr);
    void add_listener (listener_id sock, error * perr = nullptr);
    void wait_for_write (socket_id sock, error * perr = nullptr);
    void remove_socket (socket_id sock, error * perr = nullptr);
    void remove_listener (listener_id sock, error * perr = nullptr);
    bool empty () const noexcept;
    int poll (fd_set * rfds, fd_set * wfds, std::chrono::milliseconds millis, error * perr = nullptr);
};

} // namespace posix

NETTY__NAMESPACE_END
