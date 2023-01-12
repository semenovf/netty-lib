////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.01 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "pfs/netty/posix/udp_socket.hpp"
#include "pfs/i18n.hpp"
#include "pfs/netty/error.hpp"

#if _MSC_VER
#   include <winsock2.h>
#else
#   include <sys/types.h>
#   include <sys/socket.h>
#   include <netinet/in.h>
#endif

namespace netty {
namespace posix {

udp_socket::udp_socket () = default;

udp_socket::udp_socket (udp_socket && s)
    : inet_socket(std::move(s))
{}

udp_socket & udp_socket::operator = (udp_socket && s)
{
    inet_socket::operator = (std::move(s));
    return *this;
}

udp_socket::~udp_socket () = default;

}} // namespace netty::posix
