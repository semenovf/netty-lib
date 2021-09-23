////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.09.21 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <cereal/archives/portable_binary.hpp>
#include <sstream>

namespace pfs {
namespace net {
namespace p2p {

enum class envelope_flag: std::uint16_t
{
      HEAD = 0xFFFE
    , TAIL = 0xFEFF
};

// struct seal {};
struct unseal {};

class output_envelope
{
    std::ostringstream _archiver_backend;
    cereal::PortableBinaryOutputArchive _output_archive;

public:
    output_envelope ()
        : _output_archive(_archiver_backend)
    {
        _output_archive << envelope_flag::HEAD;
    }

    template <typename T>
    output_envelope & operator << (T && payload)
    {
        _output_archive << std::forward<T>(payload);
        return *this;
    }

    output_envelope & operator << (output_envelope & (*manip) (output_envelope &))
    {
        manip(*this);
        return *this;
    }

    std::string data () const
    {
        return _archiver_backend.str();
    }
};

class input_envelope
{
    bool _success {true};
    std::istringstream _archiver_backend;
    cereal::PortableBinaryInputArchive _input_archive;

public:
    input_envelope (std::string const & packet)
        : _archiver_backend(packet)
        , _input_archive(_archiver_backend)
    {
        if (_success) {
            using flag_type = std::underlying_type<envelope_flag>::type;

            std::underlying_type<envelope_flag>::type head_flag;
            _input_archive >> head_flag;

            if (head_flag != static_cast<flag_type>(envelope_flag::HEAD))
                _success = false;
        }
    }

    bool success () const noexcept
    {
        return _success;
    }

    template <typename PayloadType>
    input_envelope & operator >> (PayloadType & payload)
    {
        if (_success) {
            _input_archive >> payload;
        }

        return *this;
    }

    input_envelope & operator >> (input_envelope & (*manip) (input_envelope &))
    {
        if (_success)
            manip(*this);

        return *this;
    }

    friend input_envelope & unseal (input_envelope &);
};

inline output_envelope & seal (output_envelope & out)
{
    out << envelope_flag::TAIL;
    return out;
}

inline input_envelope & unseal (input_envelope & in)
{
    using flag_type = std::underlying_type<envelope_flag>::type;

    std::underlying_type<envelope_flag>::type tail_flag;
    in >> tail_flag;

    if (tail_flag != static_cast<flag_type>(envelope_flag::TAIL))
        in._success = false;

    return in;
}

}}} // namespace pfs::net::p2p

