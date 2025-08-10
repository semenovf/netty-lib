////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.08.04 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/netty/telemetry/visitor.hpp"
#include <msgpack.hpp>

class msgpack_serializer
{
    msgpack::sbuffer _buf;
    msgpack::packer<msgpack::sbuffer> _packer;

public:
    msgpack_serializer ()
        : _packer(_buf)
    {}

public:
    void initiate ();
    void finalize ();

    template <typename T>
    void pack (std::string const & key, T const & value)
    {
        _packer.pack(netty::telemetry::type_of<T>());
        _packer.pack(key);
        _packer.pack(value);
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

class msgpack_deserializer
{
public:
    msgpack_deserializer (char const * data, std::size_t size
        , netty::telemetry::visitor<std::string> * visitor_ptr);
};
