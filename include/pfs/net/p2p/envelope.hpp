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
#include <cereal/archives/binary.hpp>
#include <sstream>

namespace pfs {
namespace net {
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

    std::string data () const
    {
        return _archiver_backend.str();
    }
};

template <typename ArchiverBackend = std::istringstream>
class input_envelope
{
    ArchiverBackend _archiver_backend;
    cereal::BinaryInputArchive _input_archive;

public:
    input_envelope (std::string const & packet)
        : _archiver_backend(packet)
        , _input_archive(_archiver_backend)
    {}

    template <typename T>
    void unseal (T & payload)
    {
        _input_archive >> payload;
    }
};

}}} // namespace pfs::net::p2p
