////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2022 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.03.27 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "remote_file_protocol.hpp"
#include "pfs/netty/p2p/remote_file.hpp"
#include "pfs/netty/client_socket_engine.hpp"
#include "pfs/netty/default_poller_types.hpp"
#include "pfs/netty/posix/tcp_socket.hpp"
#include <cstdint>

namespace netty {
namespace p2p {

class remote_file_handle
{
public:
    using channel_type = client_socket_engine_mt<posix::tcp_socket
        , netty::client_poller_type, protocol>;
    using native_handle_type = remote_native_handle_type;
    using filesize_type = remote_file_provider::filesize_type;

private:
    native_handle_type _h {-1};
    channel_type _channel;

public:
    remote_file_handle (): _channel(channel_type::default_options()) {}
    ~remote_file_handle () {}

public: // static

    static remote_file_provider::handle_type open_read_only (
        netty::p2p::remote_path const & path, ionik::error * perr)
    {
        // FIXME
        return std::unique_ptr<remote_file_handle>{};
    }

    static remote_file_provider::handle_type open_write_only (
          netty::p2p::remote_path const & path
        , ionik::truncate_enum trunc, ionik::error * perr)
    {
        // FIXME
        return std::unique_ptr<remote_file_handle>{};
    }

    static void close (remote_file_provider::handle_type & h)
    {
        // FIXME
    }


    static filesize_type offset (remote_file_provider::handle_type const & h)
    {
        // FIXME
        return 0;
    }

    static void set_pos (remote_file_provider::handle_type & h, filesize_type offset
        , ionik::error * perr)
    {
    }

    static filesize_type read (remote_file_provider::handle_type & h, char * buffer
        , filesize_type len, ionik::error * perr)
    {
        // FIXME
        return 0;
    }

    static filesize_type write (remote_file_provider::handle_type & h
        , char const * buffer, filesize_type len, ionik::error * perr)
    {
        // FIXME
        return 0;
    }
};

}} // namespace netty::p2p
