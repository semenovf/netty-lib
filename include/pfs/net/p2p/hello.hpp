////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.09.13 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <cereal/types/string.hpp>
#include <string>

namespace pfs {
namespace net {
namespace p2p {

class hello
{
    std::string _greeting {"HELO"};

    // The TCP port the server is listening on.
    std::uint16_t _port {0};

public:
    hello ()
    {}

    explicit hello (std::string const & greeting, std::uint16_t port = 0)
        : _greeting(greeting)
        , _port(port)
    {}

    auto greeting () const noexcept -> decltype(_greeting)
    {
        return _greeting;
    }

    auto port () const noexcept -> decltype(_port)
    {
        return _port;
    }

    // Cereal requirements
    template <typename Archive>
    void serialize (Archive & ar)
    {
        ar(_greeting, ' ', _port);
    }
};

}}} // namespace pfs::net::p2p
