////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.03 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/netty/error.hpp"
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

namespace netty {
namespace posix {

class select_poller
{
public:
#if _MSC_VER
    using native_socket_type = SOCKET;
    static native_socket_type const kINVALID_SOCKET = INVALID_SOCKET;
#else
    using native_socket_type = int;
    static native_type constexpr kINVALID_SOCKET = -1;
#endif

    std::vector<native_socket_type> sockets;
    int count = 0;

    fd_set readfds;
    fd_set writefds;

    bool observe_read {false};
    bool observe_write {false};

public:
    select_poller (bool observe_read, bool observe_write);
    ~select_poller ();

    void add (native_socket_type sock, error * perr = nullptr);
    void remove (native_socket_type sock, error * perr = nullptr);
    bool empty () const noexcept;
    int poll (fd_set * rfds, fd_set * wfds, std::chrono::milliseconds millis, error * perr = nullptr);
};

}} // namespace netty::posix
