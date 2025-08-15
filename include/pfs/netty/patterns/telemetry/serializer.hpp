////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.08.07 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include "visitor.hpp"
#include <pfs/assert.hpp>
#include <pfs/binary_istream.hpp>
#include <pfs/binary_ostream.hpp>
#include <pfs/numeric_cast.hpp>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace telemetry {

template <typename KeyT>
struct key_serializer
{
    using binary_ostream_type = pfs::binary_ostream<pfs::endian::network, std::vector<char>>;

    key_serializer (binary_ostream_type & out, KeyT const & key)
    {
        out << key;
    }
};

template <>
struct key_serializer<std::string>
{
    using binary_ostream_type = pfs::binary_ostream<pfs::endian::network, std::vector<char>>;

    key_serializer (binary_ostream_type & out, std::string const & key)
    {
        out << pfs::numeric_cast<uint16_t>(key.size()) << key;
    }
};

template <typename KeyT>
class serializer
{
    using binary_ostream_type = pfs::binary_ostream<pfs::endian::network, std::vector<char>>;
    using archive_type = typename binary_ostream_type::archive_type;
    using key_type = KeyT;

private:
    archive_type _buf;

public:
    serializer ()
    {
        _buf.reserve(128);
    }

public:
    template <typename T>
    std::enable_if_t<std::is_arithmetic<T>::value, void>
    pack (key_type const & key, T const & value)
    {
        binary_ostream_type out {_buf, _buf.size()};

        out << type_of<T>();
        key_serializer<key_type>(out, key);
        out << value;
    }

    void pack (key_type const & key, string_t const & value)
    {
        binary_ostream_type out {_buf, _buf.size()};

        out << type_of<string_t>();
        key_serializer<key_type>(out, key);
        out << pfs::numeric_cast<uint16_t>(value.size()) << value;
    }

    void clear () noexcept
    {
        _buf.clear();
    }

    char const * data () const noexcept
    {
        return _buf.data();
    }

    std::size_t size () const noexcept
    {
        return _buf.size();
    }
};

template <typename KeyT>
struct key_deserializer
{
    using binary_istream_type = pfs::binary_istream<pfs::endian::network>;

    key_deserializer (binary_istream_type & in, KeyT & key)
    {
        in >> key;
    }
};

template <>
struct key_deserializer<std::string>
{
    using binary_istream_type = pfs::binary_istream<pfs::endian::network>;

    key_deserializer (binary_istream_type & in, std::string & key)
    {
        std::uint16_t key_size = 0;
        in >> key_size >> std::make_pair(& key, key_size);
    }
};

template <typename KeyT>
class deserializer
{
    using binary_istream_type = pfs::binary_istream<pfs::endian::network>;
    using key_type = KeyT;
    using visitor_type = visitor_interface<KeyT>;

public:
    deserializer (char const * data, std::size_t size, std::shared_ptr<visitor_type> vis)
    {
        binary_istream_type in {data, size};

        while (in.is_good() && in.available() > 0) {
            std::int8_t type = 0;
            key_type key;

            in >> type;
            key_deserializer<key_type>(in, key);

            if (type == type_of<string_t>()) {
                std::uint16_t value_size = 0;
                std::string value;
                in >> value_size >> std::make_pair(& value, value_size);
                vis->on(key, value);
            } else {
                switch (type) {
                    case type_of<bool>(): {
                        bool value = false;
                        in >> value;
                        vis->on(key, value);
                        break;
                    }

                    case type_of<int8_t>(): {
                        int8_t value = 0;
                        in >> value;
                        vis->on(key, value);
                        break;
                    }

                    case type_of<int16_t>(): {
                        int16_t value = 0;
                        in >> value;
                        vis->on(key, value);
                        break;
                    }

                    case type_of<int32_t>(): {
                        int32_t value = 0;
                        in >> value;
                        vis->on(key, value);
                        break;
                    }

                    case type_of<int64_t>(): {
                        int64_t value = 0;
                        in >> value;
                        vis->on(key, value);
                        break;
                    }

                    case type_of<float32_t>(): {
                        float32_t value = 0;
                        in >> value;
                        vis->on(key, value);
                        break;
                    }

                    case type_of<float64_t>(): {
                        float64_t value = 0;
                        in >> value;
                        vis->on(key, value);
                        break;
                    }

                    default:
                        vis->on_error(tr::f_("unsupported telemetry type: {}", type));
                        return;
                }
            }
        }

        PFS__THROW_UNEXPECTED(in.is_good(), "bad or corrupted telemetry data received");
    }
};

}} // namespace patterns::telemetry

NETTY__NAMESPACE_END

