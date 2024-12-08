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
#include "netty/p2p/remote_file.hpp"
#include "netty/client_socket_engine.hpp"
#include "netty/default_poller_types.hpp"
#include "netty/posix/tcp_socket.hpp"
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
        // FIXME Implement
        return std::unique_ptr<remote_file_handle>{};
    }

    static remote_file_provider::handle_type open_write_only (
          netty::p2p::remote_path const & path
        , ionik::truncate_enum trunc, ionik::error * perr)
    {
        // FIXME Implement
        return std::unique_ptr<remote_file_handle>{};
    }

    static void close (remote_file_provider::handle_type & h)
    {
        // FIXME Implement
    }


    static remote_file_provider::offset_result_type offset (
          remote_file_provider::handle_type const & h
        , ionik::error * perr)
    {
        // FIXME Implement
        return remote_file_provider::offset_result_type{0, false};
    }

    static bool set_pos (remote_file_provider::handle_type & h, filesize_type offset
        , ionik::error * perr)
    {
        // FIXME Implement
        return false;
    }

    static remote_file_provider::read_result_type read (remote_file_provider::handle_type & h
        , char * buffer, filesize_type len, ionik::error * perr)
    {
        // FIXME Implement
        return remote_file_provider::read_result_type{0, false};
    }

    static remote_file_provider::write_result_type write (remote_file_provider::handle_type & h
        , char const * buffer, filesize_type len, ionik::error * perr)
    {
        // FIXME Implement
        return remote_file_provider::write_result_type{0, false};
    }
};

}} // namespace netty::p2p
