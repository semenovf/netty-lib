////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2026 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2026.04.26 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "ssl/tls_socket.hpp"
#include "posix/tcp_socket.hpp"
#include <openssl/ssl.h>
#include <openssl/err.h>

NETTY__NAMESPACE_BEGIN

namespace ssl {

struct tls_socket::impl: public posix::tcp_socket
{
    SSL * ssl {nullptr};
    // SSL_CTX * ctx {nullptr};
    tls_options opts;

    impl (): posix::tcp_socket() {}
    impl (posix::tcp_socket && ts): posix::tcp_socket(std::move(ts)) {}
};

} // namespace ssl

NETTY__NAMESPACE_END
