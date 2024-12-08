////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.05.07 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "message.hpp"
#include <pfs/string_view.hpp>
#include <vector>

struct basic_envelope
{
    static constexpr char start_flag = 0x01;
    static constexpr char end_flag   = 0x02;
};

template <typename Deserializer>
class input_envelope: public basic_envelope
{
public:
    using deserializer_type = Deserializer;
    using archive_type = std::vector<char>;

private:
    message_enum _type {message_enum::bad};
    archive_type _payload;

public:
    input_envelope (Deserializer & in)
    {
        char x, y;

        in >> x >> _type >> _payload >> y;

        if (x != start_flag) {
            _type = message_enum::bad;
            return;
        }

        if (y != end_flag) {
            _type = message_enum::bad;
            return;
        }
    }

    operator bool () const noexcept
    {
        return _type != message_enum::bad;
    }

    archive_type const & payload () const
    {
        return _payload;
    }

    message_enum message_type () const noexcept
    {
        return _type;
    }
};

template <typename Serializer>
class output_envelope: public basic_envelope
{
public:
    using archive_type = typename Serializer::archive_type;

private:
    Serializer _out;

public:
    output_envelope (message_enum type, archive_type const & payload)
    {
        _out << start_flag << type << payload << end_flag;
    }

public:
    archive_type take ()
    {
        return _out.take();
    }
};

constexpr char basic_envelope::start_flag;
constexpr char basic_envelope::end_flag;
