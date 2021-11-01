////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.10.26 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/net/inet4_addr.hpp"
#include "pfs/emitter.hpp"
#include "pfs/fmt.hpp"
#include <string>
#include <vector>

namespace pfs {
namespace net {
namespace p2p {
namespace udt {

class udp_socket
{
    // Typedef UDTSOCKET as defined in `udt.h`
    using UDTSOCKET = int;

public:
    // Must be same as UDTSTATUS defined in `udt.h`
    enum status_enum {INIT = 1
        , OPENED
        , LISTENING
        , CONNECTING
        , CONNECTED
        , BROKEN
        , CLOSING
        , CLOSED
        , NONEXIST
    };

    using id_type = UDTSOCKET;

private:
    UDTSOCKET _socket {-1};

public:
    pfs::emitter_mt<std::string const &> failure;

public:
    udp_socket () = default;
    ~udp_socket ();

    udp_socket (udp_socket const &) = delete;
    udp_socket & operator = (udp_socket const &) = delete;

    udp_socket (udp_socket &&) = default;
    udp_socket & operator = (udp_socket &&) = default;

    id_type id () const
    {
        return _socket;
    }

    status_enum state () const;

    bool bind (inet4_addr const & addr, std::uint16_t port);
    bool listen (int backlog = 10);
    udp_socket accept (inet4_addr * addr, std::uint16_t * port);
    bool connect (inet4_addr const & addr, std::uint16_t port);
    void close ();

    std::string error_string () const;

    //std::vector<char> recvmsg ();
    std::streamsize send (char const * data, std::streamsize len);
};

}}}} // namespace pfs::net::p2p::udt
