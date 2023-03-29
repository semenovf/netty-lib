////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2022 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.03.29 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "remote_file_protocol.hpp"
#include "pfs/netty/p2p/remote_file.hpp"
#include "pfs/binary_istream.hpp"
#include "pfs/binary_ostream.hpp"

#include "pfs/netty/tag.hpp"
#include "pfs/log.hpp"

namespace netty {
namespace p2p {

using binary_istream = pfs::binary_istream<std::uint32_t, pfs::endian::network>;
using binary_ostream = pfs::binary_ostream<std::uint32_t, pfs::endian::network>;

std::vector<char> protocol::serialize_envelope (std::vector<char> const & payload)
{
    std::vector<char> envelope;
    binary_ostream out {envelope};
    out << BEGIN
        << payload // <- payload size here
        << crc16_field_type{0}
        << END;
    return envelope;
}

void protocol::process_payload (std::vector<char> const & payload)
{
    LOGD("", "=== PROCESS PAYLOAD ===");

    operation_field_type op;
    method_field_type m;

    binary_istream in {payload.data(), payload.size()};

    in >> op >> m;

    switch (static_cast<operation_enum>(op)) {
        case operation_enum::request:
            break;
        case operation_enum::response:
            switch(static_cast<method_enum>(m)) {
                case method_enum::select_file:
                    LOGD(TAG, "=== PROCESS PAYLOAD: FILE SELECTED ===");
                    break;
                case method_enum::open_read_only:
                    break;
                case method_enum::open_write_only:
                    break;
                case method_enum::close:
                    break;
                case method_enum::offset:
                    break;
                case method_enum::set_pos:
                    break;
                case method_enum::read:
                    break;
                case method_enum::write:
                    break;
                default:
                    LOGE(TAG, "Bad response type");
                    break;
            }
            break;
        case operation_enum::notification:
            break;
        case operation_enum::error: // Unused yet
            break;
        default:
            LOGE(TAG, "Bad operation type");
            break;
    }
}

template <>
std::vector<char>
protocol::serialize (select_file_request const & p)
{
    std::vector<char> payload;
    binary_ostream out {payload};
    out << operation_enum::request << method_enum::select_file << next_rid();
    return serialize_envelope(payload);
}

template <>
std::vector<char>
protocol::serialize (open_read_only_request const & p)
{
    std::vector<char> payload;
    binary_ostream out {payload};
    out << operation_enum::request << method_enum::open_read_only << next_rid() << p.uri;
    return serialize_envelope(payload);
}

template <>
std::vector<char>
protocol::serialize (open_write_only_request const & p)
{
    std::vector<char> payload;
    binary_ostream out {payload};
    out << operation_enum::request << method_enum::open_write_only << next_rid() << p.uri << p.trunc;
    return serialize_envelope(payload);
}

template <>
std::vector<char>
protocol::serialize (close_notification const & p)
{
    std::vector<char> payload;
    binary_ostream out {payload};
    out << operation_enum::notification << method_enum::close << p.h;
    return serialize_envelope(payload);
}

template <>
std::vector<char>
protocol::serialize (offset_request const & p)
{
    std::vector<char> payload;
    binary_ostream out {payload};
    out << operation_enum::request << method_enum::offset << next_rid() << p.h;
    return serialize_envelope(payload);
}

template <>
std::vector<char>
protocol::serialize (set_pos_notification const & p)
{
    std::vector<char> payload;
    binary_ostream out {payload};
    out << operation_enum::notification << method_enum::set_pos << p.h << p.offset;
    return serialize_envelope(payload);
}

template <>
std::vector<char>
protocol::serialize (read_request const & p)
{
    std::vector<char> payload;
    binary_ostream out {payload};
    out << operation_enum::request << method_enum::read << next_rid() << p.h << p.len;
    return serialize_envelope(payload);
}

template <>
std::vector<char>
protocol::serialize (write_request const & p)
{
    std::vector<char> payload;
    binary_ostream out {payload};
    out << operation_enum::request << method_enum::write << next_rid() << p.h << p.data;
    return serialize_envelope(payload);
}

bool protocol::has_complete_packet (char const * data, std::size_t len)
{
    if (len < MIN_PACKET_SIZE)
        return false;

    char b;
    std::uint32_t sz;
    binary_istream in {data, data + len};
    in >> b >> sz;

    return MIN_PACKET_SIZE + sz <= len;
}

std::pair<bool, std::size_t> protocol::exec (char const * data, std::size_t len)
{
    if (len == 0)
        return std::make_pair(true, 0);

    binary_istream in {data, data + len};
    std::vector<char> payload;
    char b, e;
    crc16_field_type crc16;

    in >> b >> payload >> crc16 >> e;

    bool success = (b == BEGIN && e == END);

    if (!success)
        return std::make_pair(false, 0);

    process_payload(payload);

    return std::make_pair(true, static_cast<std::size_t>(in.begin() - data));
}

}} // namespace netty.p2p
