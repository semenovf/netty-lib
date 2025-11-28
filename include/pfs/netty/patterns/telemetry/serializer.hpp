////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.08.07 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "visitor.hpp"
#include <pfs/assert.hpp>
#include <pfs/numeric_cast.hpp>
#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace telemetry {

namespace details {

template <typename KeyT, typename Serializer>
struct key_serializer
{
    key_serializer (Serializer & out, KeyT const & key)
    {
        out << key;
    }
};

template <typename Serializer>
struct key_serializer<string_t, Serializer>
{
    key_serializer (Serializer & out, string_t const & key)
    {
        out << pfs::numeric_cast<uint16_t>(key.size());
        out << key;
    }
};

template <typename ValueT, typename Serializer>
struct value_serializer
{
    value_serializer (Serializer & out, ValueT const & value)
    {
        out << value;
    }
};

template <typename Serializer>
struct value_serializer<string_t, Serializer>
{
    value_serializer (Serializer & out, string_t const & value)
    {
        out << pfs::numeric_cast<uint16_t>(value.size());
        out << value;
    }
};

template <typename KeyT, typename Deserializer>
struct key_deserializer
{
    key_deserializer (Deserializer & in, KeyT & key)
    {
        in >> key;
    }
};

template <typename Deserializer>
struct key_deserializer<string_t, Deserializer>
{
    key_deserializer (Deserializer & in, string_t & key)
    {
        std::uint16_t key_size = 0;
        in >> key_size;
        in.read(key, key_size);
    }
};

template <typename ValueT, typename Deserializer>
struct value_deserializer
{
    value_deserializer (Deserializer & in, ValueT & value)
    {
        in >> value;
    }
};

template <typename Deserializer>
struct value_deserializer<string_t, Deserializer>
{
    value_deserializer (Deserializer & in, string_t & value)
    {
        std::uint16_t value_size = 0;
        in >> value_size;
        in.read(value, value_size);
    }
};

} // namespace details

template <typename KeyT, typename Serializer>
class key_value_serializer
{
public:
    template <typename T>
    key_value_serializer (Serializer & out, KeyT const & key, T const & value)
    {
        out << type_of<T>();
        details::key_serializer<KeyT, Serializer>(out, key);
        details::value_serializer<T, Serializer>(out, value);
    }
};

template <typename KeyT, typename Deserializer>
class key_value_deserializer
{
    using key_type = KeyT;
    using visitor_type = visitor_interface<KeyT>;

public:
    key_value_deserializer (char const * data, std::size_t size, std::shared_ptr<visitor_type> visitor)
    {
        Deserializer in {data, size};

        while (in.is_good() && in.available() > 0) {
            std::int8_t type = 0;
            key_type key;

            in >> type;
            details::key_deserializer<key_type, Deserializer>(in, key);

            switch (type) {
                case type_of<string_t>():  read_and_visit<string_t>(in, key, visitor); break;
                case type_of<bool>():      read_and_visit<bool>(in, key, visitor); break;
                case type_of<int8_t>():    read_and_visit<int8_t>(in, key, visitor); break;
                case type_of<int16_t>():   read_and_visit<int16_t>(in, key, visitor); break;
                case type_of<int32_t>():   read_and_visit<int32_t>(in, key, visitor); break;
                case type_of<int64_t>():   read_and_visit<int64_t>(in, key, visitor); break;
                case type_of<float32_t>(): read_and_visit<float32_t>(in, key, visitor); break;
                case type_of<float64_t>(): read_and_visit<float64_t>(in, key, visitor); break;
                default:
                    visitor->on_error(tr::f_("unsupported telemetry type={} for key={}", type, key));
                    return;
            }
        }

        PFS__THROW_UNEXPECTED(in.is_good(), "bad or corrupted telemetry data received");
    }

private:
    template <typename T>
    void read_and_visit (Deserializer & in, key_type const & key, std::shared_ptr<visitor_type> & visitor)
    {
        T value;
        details::value_deserializer<T, Deserializer>(in, value);

        if (in.is_good())
            visitor->on(key, value);
    }
};

} // namespace telemetry

NETTY__NAMESPACE_END
