////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.05.05 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/log.hpp"
#include <string>

struct client_connection_context;
struct server_connection_context;

enum class message_enum: std::uint8_t
{
      bad = 0
    , echo = 1
};

struct echo
{
    std::string text;
};

template <typename M>
message_enum message_typify ();

template <>
constexpr message_enum message_typify<echo> () { return message_enum::echo; }

template <typename Serializer>
class message_serializer
{
public:
    using archive_type = typename Serializer::archive_type;

private:
    Serializer _out;

public:
    message_serializer () = default;
    ~message_serializer () = default;

    template <typename M>
    message_serializer (M && msg)
    {
        _out << msg;
    }

public:
    archive_type take ()
    {
        return _out.take();
    }
};

template <typename ConnectionContext, typename Deserializer>
class message_processor
{
private:
    template <typename M>
    bool parse (ConnectionContext & conn, Deserializer & in)
    {
        M m;
        in >> m;
        return in.is_good() && process(conn, m);
    }

public:
    message_processor () = default;
    ~message_processor () = default;

    bool parse (ConnectionContext & conn, message_enum type, char const * begin, char const * end)
    {
        Deserializer in {begin, end};

        switch (type) {
            case message_enum::echo: return parse<echo>(conn, in);
            default: break;
        }

        return false;
    }
};
