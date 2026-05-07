////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2026 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2026.04.26 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "ssl/tls_socket.hpp"
#include "error.hpp"
#include "posix/tcp_socket.hpp"
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <sstream>

NETTY__NAMESPACE_BEGIN

namespace ssl {

struct tls_socket::impl: public posix::tcp_socket
{
    SSL * ssl {nullptr};
    SSL_CTX * ctx {nullptr};
    // tls_options opts;

    impl (): posix::tcp_socket() {}
    impl (posix::tcp_socket && ts): posix::tcp_socket(std::move(ts)) {}

    ~impl ()
    {
        if (ssl != nullptr) {
            SSL_shutdown(ssl);
            SSL_free(ssl);
            ssl = nullptr;
        }

        if (ctx != nullptr) {
            SSL_CTX_free(ctx);
            ctx = nullptr;
        }
    }

    std::string dump_cipher () const
    {
        std::ostringstream out;
        char const * version = SSL_get_version(ssl);
        out << "TLS version: " << version;

        SSL_SESSION const * session = SSL_get_session(ssl);
        SSL_CIPHER const * cipher = SSL_SESSION_get0_cipher(session);
        out << "\nCIPHER is " << SSL_CIPHER_get_name(cipher);
        return out.str();
    }
};

inline error get_ssl_error (int ssl_errn, std::string const & desc)
{
    if (ssl_errn == SSL_ERROR_SYSCALL)
        return error {pfs::get_last_system_error(), desc};

    return error {
          make_error_code(errc::ssl_error)
        , fmt::format("{}: {}", desc, ERR_error_string(ssl_errn, nullptr))
    };
}

/**
 * Creates context and loads certificates and private keys according to passed TLS options.
 *
 * @return Context or @c nullptr on error.
 */
SSL_CTX * create_ssl_context (bool is_listener, tls_options const & opts, error * perr);

} // namespace ssl

NETTY__NAMESPACE_END
