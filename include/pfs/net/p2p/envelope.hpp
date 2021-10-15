////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.09.21 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/crc32.hpp"
#include <cereal/archives/portable_binary.hpp>
#include <sstream>

namespace pfs {
namespace net {
namespace p2p {

template <typename ArchiverBackend = std::ostringstream>
class output_envelope
{
    ArchiverBackend _archiver_backend;
    cereal::PortableBinaryOutputArchive _output_archive;

public:
    output_envelope () : _output_archive(_archiver_backend
        , cereal::PortableBinaryOutputArchive::Options::BigEndian())
    {}

    template <typename T>
    void seal (T && payload)
    {
        _output_archive << std::forward<T>(payload);
    }

    std::string data () const
    {
        return _archiver_backend.str();
    }
};

template <typename ArchiverBackend = std::istringstream>
class input_envelope
{
    ArchiverBackend _archiver_backend;
    cereal::PortableBinaryInputArchive _input_archive;

public:
    input_envelope (std::string const & packet)
        : _archiver_backend(packet)
        , _input_archive(_archiver_backend)
    {
//         if (_success) {
//             using flag_type = std::underlying_type<envelope_flag>::type;
//
//             std::underlying_type<envelope_flag>::type head_flag;
//             _input_archive >> head_flag;
//
//             if (head_flag != static_cast<flag_type>(envelope_flag::HEAD))
//                 _success = false;
//         }
    }

//     bool success () const noexcept
//     {
//         return _success;
//     }
//
//     std::int32_t crc32 () const noexcept
//     {
//         return _crc32;
//     }
//
//     template <typename PayloadType>
//     input_envelope & operator >> (PayloadType & payload)
//     {
//         if (_success) {
//             _input_archive >> payload;
//             _crc32 = crc32_of(payload, _crc32);
//         }
//
//         return *this;
//     }
//
//     input_envelope & operator >> (input_envelope & (*manip) (input_envelope &))
//     {
//         if (_success)
//             manip(*this);
//
//         return *this;
//     }
//
//     input_envelope & unseal ()
//     {
//         using flag_type = std::underlying_type<envelope_flag>::type;
//
//         std::int32_t crc32;
//         flag_type tail_flag;
//
//         _input_archive >> crc32 >> tail_flag;
//
//         if (tail_flag != static_cast<flag_type>(envelope_flag::TAIL))
//             _success = false;
//
//         if (crc32 != _crc32)
//             _success = false;
//
//         return *this;
//     }
};

// template <typename ArchiverBackend = std::ostringstream>
// inline output_envelope<ArchiverBackend> & seal (output_envelope<ArchiverBackend> & out)
// {
//     return out.seal();
// }
//
// template <typename ArchiverBackend = std::istringstream>
// inline input_envelope<ArchiverBackend> & unseal (input_envelope<ArchiverBackend> & in)
// {
//     return in.unseal();
// }

}}} // namespace pfs::net::p2p

namespace pfs {

// template <>
// inline int32_t crc32_of<net::p2p::envelope_flag> (net::p2p::envelope_flag const & flag
//     , int32_t initial)
// {
//     return crc32_of(static_cast<std::underlying_type<net::p2p::envelope_flag>::type>(flag), initial);
// }

} // namespace pfs
