////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.09.20 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "endpoint.hpp"
#include "pfs/emitter.hpp"
#include <chrono>
#include <string>

namespace pfs {
namespace net {
namespace p2p {

template <typename Derived>
class basic_listener
{
public:
    struct options
    {
        // Address to bind listener ('*' is any address)
        std::string listener_addr4 {"*"};
        std::uint16_t listener_port {42424};
        std::string listener_interface {"*"};
    };

public: // signals
    pfs::emitter_mt<std::string const & /*error*/> failure;
    pfs::emitter_mt<> accepted;

protected:
    basic_listener () {}
    ~basic_listener () {}

public:
    bool set_options (options && opts)
    {
        return static_cast<Derived *>(this)->set_options_impl(std::move(opts));
    }

    bool start ()
    {
        return static_cast<Derived *>(this)->start_impl();
    }

    void stop ()
    {
        static_cast<Derived *>(this)->stop_impl();
    }

    bool started () const noexcept
    {
        return static_cast<Derived *>(this)->started_impl();
    }
};

}}} // namespace pfs::net::p2p
