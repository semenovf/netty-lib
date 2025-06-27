////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.06.27 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "netty/inet4_addr.hpp"
#include <pfs/endian.hpp>
#include <pfs/i18n.hpp>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

namespace netty {

std::vector<inet4_addr> inet4_addr::resolve (std::string const & hostname, error * perr)
{
    struct addrinfo hints;
    struct addrinfo * ai = nullptr;

    std::memset(& hints, 0, sizeof(hints));

    hints.ai_family = AF_INET; // Allow IPv4 only
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = 0;
    hints.ai_protocol = 0;     // Any protocol

    int rc = getaddrinfo(hostname.c_str(), nullptr, & hints, & ai);

    if (rc != 0) {
        pfs::throw_or(perr, make_error_code(pfs::errc::system_error)
            , tr::f_("resolve host failure: {}: {}", hostname, gai_strerror(rc)));
        return std::vector<inet4_addr>{};
    }

    std::vector<inet4_addr> result;

    for (struct addrinfo * p = ai; p != nullptr; p = p->ai_next) {
        void * addr = nullptr;
        char ip [INET_ADDRSTRLEN];

        if (p->ai_family == AF_INET) {
            struct sockaddr_in * ipv4 = reinterpret_cast<struct sockaddr_in *>(p->ai_addr);
            result.push_back(inet4_addr{pfs::to_native_order(ipv4->sin_addr.s_addr)});
        }
    }

    freeaddrinfo(ai);

    return result;
}

} // namespace netty
