////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2021.09.21 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <sstream>
#include <utility>

namespace netty {
namespace p2p {

// NOTE!
// Do not use PortableBinaryOutputArchive and PortableBinaryInputArchive
// as they add flag corresponding to endianess.

template <typename ArchiverBackend = std::ostringstream>
class output_envelope
{
    ArchiverBackend _archiver_backend;
    cereal::BinaryOutputArchive _output_archive;

public:
    output_envelope () : _output_archive(_archiver_backend)
    {}

    template <typename T>
    void seal (T && payload)
    {
        _output_archive << std::forward<T>(payload);
    }

    template <typename T>
    output_envelope & operator << (T && payload)
    {
        _output_archive << std::forward<T>(payload);
        return *this;
    }

    std::string data () const
    {
        return _archiver_backend.str();
    }
};

class basic_input_envelope
{
protected:
    cereal::BinaryInputArchive _input_archive;

protected:
    basic_input_envelope (std::istream & archiver_backend)
        : _input_archive(archiver_backend)
    {}

public:
    template <typename T>
    void unseal (T & payload)
    {
        _input_archive >> payload;
    }
    
    template <typename T>
    basic_input_envelope & operator >> (T & payload)
    {
        _input_archive >> payload;
        return *this;
    }
};

template <typename ArchiverBackend = std::istream>
class input_envelope;

template <>
class input_envelope<std::istream>: public basic_input_envelope
{
    struct buffer : public std::streambuf
    {
        buffer (char const * s, std::size_t n)
        {
            auto * p = const_cast<char*>(s);
            setg(p, p, p + n);
        }
    };

private:
    buffer _buf;
    std::istream _archiver_backend;

public:
    input_envelope (std::string & packet)
        : basic_input_envelope(_archiver_backend)
        , _buf(packet.data(), packet.size())
        , _archiver_backend(& _buf)
    {
        // NOTE. Avoid use of std::istream::rdbuf()->pubsetbuf(const_cast<char *>(data), size)
        // This work with GCC, but not with MSVC.
    }

    input_envelope (char const * data, std::streamsize size)
        : basic_input_envelope(_archiver_backend)
        , _buf(data, size)
        , _archiver_backend(&_buf)
    {}

    template <typename T>
    void unseal (T & payload)
    {
        _input_archive >> payload;
    }

    template <typename T>
    input_envelope & operator >> (T & payload)
    {
        _input_archive >> payload;
        return *this;
    }
};

}} // namespace netty::p2p
