////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.08.04 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "msgpack_serializer.hpp"
#include <pfs/assert.hpp>
#include <pfs/i18n.hpp>

#if 0
/**
 * Calculates msgpack buffer size for string value only
 */
static std::size_t calculate_string_size (std::size_t len)
{
    if (len <= 31)
        return 1 + len;

    if (len <= 255)
        return 2 + len;

    if (len <= 65535)
        return 3 + len;

    return 5 + len;
}

/**
 * Calculates msgpack buffer size for specified string lengths.
 */
std::size_t calculate_string_array_size (std::vector<size_t> const & lengths)
{
    std::size_t total = 0;

    // Array header
    std::size_t count = lengths.size();

    if (count <= 15)
        total += 1;
    else if (count <= 65535)
        total += 3;
    else
        total += 5;

    // Each string's size
    for (std::size_t len : lengths) {
        if (len <= 31)
            total += 1 + len;
        else if (len <= 255)
            total += 2 + len;
        else if (len <= 65535)
            total += 3 + len;
        else
            total += 5 + len;
    }

    return total;
}
#endif

void msgpack_serializer::initiate ()
{
    _buf.clear();

    // Write raw byte
    std::uint8_t byte = 0xBE;
    _buf.write(reinterpret_cast<char const *>(& byte), 1);
}

void msgpack_serializer::finalize ()
{
    // Write raw byte
    std::uint8_t byte = 0xED;
    _buf.write(reinterpret_cast<char const *>(& byte), 1);
}

msgpack_deserializer::msgpack_deserializer (char const * data, std::size_t size
    , netty::telemetry::visitor<std::string> * visitor_ptr)
{
    if (size < 2) {
        visitor_ptr->on_error(tr::_("Bad packet: received data size too small, ignored"));
        return;
    }

    if (static_cast<std::uint8_t>(data[0]) != 0xBE) {
        visitor_ptr->on_error(tr::_("Bad packet: expected begin flag, ignored"));
        return;
    }

    data++;
    size--;

    if (static_cast<std::uint8_t>(data[size - 1]) != 0xED) {
        visitor_ptr->on_error(tr::_("Bad packet: expected end flag, ignored"));
        return;
    }

    size--;

    std::size_t offset = 0;
    msgpack::object_handle oh;

    while (offset < size) {
        msgpack::unpack(oh, data, size, offset);
        std::int8_t type = oh.get().as<std::int8_t>();

        msgpack::unpack(oh, data, size, offset);
        auto key = oh.get().as<std::string>();

        msgpack::unpack(oh, data, size, offset);

        switch (type) {
            case netty::telemetry::type_of<netty::telemetry::string_t>(): {
                auto value = oh.get().as<netty::telemetry::string_t>();
                visitor_ptr->on(key, value);
                break;
            }

            case netty::telemetry::type_of<netty::telemetry::int8_t>(): {
                auto value = oh.get().as<netty::telemetry::int8_t>();
                visitor_ptr->on(key, value);
                break;
            }

            case netty::telemetry::type_of<netty::telemetry::int16_t>(): {
                auto value = oh.get().as<netty::telemetry::int16_t>();
                visitor_ptr->on(key, value);
                break;
            }

            case netty::telemetry::type_of<netty::telemetry::int32_t>(): {
                auto value = oh.get().as<netty::telemetry::int32_t>();
                visitor_ptr->on(key, value);
                break;
            }

            case netty::telemetry::type_of<netty::telemetry::int64_t>(): {
                auto value = oh.get().as<netty::telemetry::int64_t>();
                visitor_ptr->on(key, value);
                break;
            }

            case netty::telemetry::type_of<netty::telemetry::float32_t>(): {
                auto value = oh.get().as<netty::telemetry::float32_t>();
                visitor_ptr->on(key, value);
                break;
            }

            case netty::telemetry::type_of<netty::telemetry::float64_t>(): {
                auto value = oh.get().as<netty::telemetry::float64_t>();
                visitor_ptr->on(key, value);
                break;
            }

            default:
                visitor_ptr->on_error(tr::f_("Unsupported type: {}", type));
                return;
        }
    }

    PFS__THROW_UNEXPECTED(offset == size, "Fix msgpack_deserializer algorithm");
}
