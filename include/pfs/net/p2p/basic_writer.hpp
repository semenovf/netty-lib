////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.10.10 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "packet.hpp"
#include "pfs/emitter.hpp"

namespace pfs {
namespace net {
namespace p2p {

template <typename Impl>
class basic_writer
{
public: // signals
    pfs::emitter_mt<std::string const &> failure;

protected:
    basic_writer () = default;
    ~basic_writer () = default;

    basic_writer (basic_writer const &) = delete;
    basic_writer & operator = (basic_writer const &) = delete;

    basic_writer (basic_writer &&) = default;
    basic_writer & operator = (basic_writer &&) = default;

public:
    std::streamsize write (inet4_addr const & host, std::uint32_t port
        , char const * data, std::streamsize size)
    {
        return static_cast<Impl *>(this)->write_impl(host, port, data, size);
    }
};

}}} // namespace pfs::net::p2p
