////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.08.05 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/netty/telemetry/visitor.hpp"
#include <pfs/assert.hpp>
#include <QByteArray>
#include <QDataStream>
#include <limits>

class qt_serializer
{
    QByteArray _buf;
    QDataStream _out;

public:
    qt_serializer ()
        : _out(& _buf, QIODeviceBase::WriteOnly)
    {}

public:
    void initiate ();
    void finalize ();

    template <typename T>
    void pack (std::string const & key, T const & value)
    {
        pack(_out, netty::telemetry::type_of<T>());
        pack(_out, key);
        pack(_out, value);
    }

    char const * data () const noexcept
    {
        return _buf.constData();
    }

    std::size_t size () const noexcept
    {
        return _buf.size();
    }

private:
    /**
     * @note There are specializations for std::string and std::int64_t.
     */
    template <typename T>
    static void pack (QDataStream & out, T const & value)
    {
        out << value;
    }
};

template <>
void qt_serializer::pack<std::string> (QDataStream & out, std::string const & value);

template <>
void qt_serializer::pack<std::int64_t> (QDataStream & out, std::int64_t const & value);

class qt_deserializer
{
public:
    qt_deserializer (char const * data, std::size_t size
        , netty::telemetry::visitor<std::string> * visitor_ptr);
};

