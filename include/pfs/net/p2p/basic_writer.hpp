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
// #include "pfs/uuid.hpp"
// #include "pfs/emitter.hpp"
// #include "pfs/net/inet4_addr.hpp"
// #include <string>

namespace pfs {
namespace net {
namespace p2p {

template <typename Impl, std::size_t PacketSize>
class basic_writer
{
protected:
    using packet_type = packet<PacketSize>;

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
    void write (inet4_addr const & host, std::uint32_t port, packet_type const & packet)
    {
        static_cast<Impl *>(this)->write_impl(host, port, packet);
    }
};

}}} // namespace pfs::net::p2p
