////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2021.09.21 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <sstream>
#include <utility>

#if NETTY_P2P__CEREAL_ENABLED
#   include <cereal/archives/binary.hpp>
#   include <cereal/types/string.hpp>
#endif

namespace netty {
namespace p2p {

#if NETTY_P2P__CEREAL_ENABLED
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

    // Clear content of internal archive
    void reset ()
    {
        _archiver_backend.str(std::string{});
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
            auto * p = const_cast<char *>(s);
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

    input_envelope (char const * data, std::size_t size)
        : basic_input_envelope(_archiver_backend)
        , _buf(data, size)
        , _archiver_backend(& _buf)
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

    /**
     * Peeks character from underlying istream instance.
     *
     * @return character (>= 0) peek from stream or negative value on failure.
     */
    int peek ()
    {
        return _archiver_backend.good()
            ? _archiver_backend.peek()
            : -1;
    }

    template <typename P>
    static void unseal (P & payload, std::vector<char> const & buffer)
    {
        input_envelope in {buffer.data(), buffer.size()};
        in.unseal(payload);
    }

    template <typename P>
    static P unseal (std::vector<char> const & buffer)
    {
        P payload;
        input_envelope{buffer.data(), buffer.size()}.unseal(payload);
        return payload;
    }

    template <typename P>
    static P unseal (char const * data, std::size_t size)
    {
        P payload;
        input_envelope{data, size}.unseal(payload);
        return payload;
    }
};

#endif

}} // namespace netty::p2p
