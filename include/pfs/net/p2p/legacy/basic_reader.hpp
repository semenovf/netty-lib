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
#include "pfs/uuid.hpp"
#include "pfs/emitter.hpp"
#include "pfs/net/inet4_addr.hpp"
#include <string>

namespace pfs {
namespace net {
namespace p2p {

template <typename Impl>
class basic_reader
{
public:
    struct options
    {
        // Address to bind listener (inet4_addr{} is any address)
        inet4_addr listener_addr4;
        std::uint16_t listener_port {0};
        std::string listener_interface {"*"};
    };

public: // signals
    pfs::emitter_mt<char const *, std::streamsize> datagram_received;
    pfs::emitter_mt<std::string const &> failure;

protected:
    basic_reader () = default;
    ~basic_reader () = default;

    basic_reader (basic_reader const &) = delete;
    basic_reader & operator = (basic_reader const &) = delete;

    basic_reader (basic_reader &&) = default;
    basic_reader & operator = (basic_reader &&) = default;

public:
    bool set_options (options && opts)
    {
        return static_cast<Impl *>(this)->set_options_impl(std::move(opts));
    }

    bool start ()
    {
        return static_cast<Impl *>(this)->start_impl();
    }

    void stop ()
    {
        static_cast<Impl *>(this)->stop_impl();
    }

    bool started () const noexcept
    {
        return static_cast<Impl const *>(this)->started_impl();
    }

    inet4_addr address () const noexcept
    {
        return static_cast<Impl const *>(this)->address_impl();
    }

    std::uint16_t port () const noexcept
    {
        return static_cast<Impl const *>(this)->port_impl();
    }
};

}}} // namespace pfs::net::p2p

