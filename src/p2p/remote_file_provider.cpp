////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2022 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.03.27 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "remote_file_handle.hpp"
#include "pfs/chrono_literals.hpp"

#include "pfs/log.hpp"

namespace netty {
namespace p2p {

remote_path select_remote_file (socket4_addr provider_saddr
    , std::chrono::seconds wait_timeout)
{
    remote_file_handle::channel_type channel;
    remote_path result;

//     channel.connected = [wait_timeout, & result] (remote_file_handle::channel_type & self) {
//         self.send(select_file_request{});
//         auto res = self.recv(wait_timeout);
//
//         if (res == remote_file_handle::channel_type::loop_result::success) {
//             LOGD("", "=== SELECT REMOTE FILE SUCCESS ===");
//         }
//     };

    channel.connection_refused = [] (remote_file_handle::channel_type &) {
        LOGD("", "=== CONNECTION REFUSED ===");
    };

    if (!channel.connect(provider_saddr, 1_s))
        return remote_path{};

    channel.send(select_file_request{}, 1_s);
    auto res = channel.recv(wait_timeout);

    // FIXME
    return remote_path{};
}

}} // namespace netty::p2p

namespace ionik {

using file_provider_t = file_provider<std::unique_ptr<netty::p2p::remote_file_handle>
    , netty::p2p::remote_path>;
using filepath_t = file_provider_t::filepath_type;
using filesize_t = file_provider_t::filesize_type;
using handle_t   = file_provider_t::handle_type;

template <>
handle_t file_provider_t::invalid () noexcept
{
    return handle_t{};
}

template <>
bool file_provider_t::is_invalid (handle_type const & h) noexcept
{
    return !h;
}

template <>
handle_t file_provider_t::open_read_only (filepath_t const & path, error * perr)
{
    return netty::p2p::remote_file_handle::open_read_only(path, perr);
}

template <>
handle_t file_provider_t::open_write_only (filepath_t const & path
    , truncate_enum trunc, error * perr)
{
    return netty::p2p::remote_file_handle::open_write_only(path, trunc, perr);
}

template <>
void file_provider_t::close (handle_t & h)
{
    netty::p2p::remote_file_handle::close(h);
}

template <>
filesize_t file_provider_t::offset (handle_t const & h)
{
    return netty::p2p::remote_file_handle::offset(h);
}

template <>
void file_provider_t::set_pos (handle_t & h, filesize_t offset, error * perr)
{
    netty::p2p::remote_file_handle::set_pos(h, offset, perr);
}

template <>
filesize_t file_provider_t::read (handle_t & h, char * buffer, filesize_t len
    , error * perr)
{
    return netty::p2p::remote_file_handle::read(h, buffer, len, perr);
}

template <>
filesize_t file_provider_t::write (handle_t & h, char const * buffer
    , filesize_t len, error * perr)
{
    return netty::p2p::remote_file_handle::write(h, buffer, len, perr);
}

} // namespace ionik
