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
#include <pfs/optional.hpp>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <sstream>

NETTY__NAMESPACE_BEGIN

namespace ssl {

struct tls_socket::impl: public posix::tcp_socket
{
    SSL * ssl {nullptr};
    SSL_CTX * ctx {nullptr};

    impl (): posix::tcp_socket() {}
    impl (posix::tcp_socket && ts): posix::tcp_socket(std::move(ts)) {}

    impl (impl && other) noexcept: posix::tcp_socket(std::move(other))
    {
        ssl = other.ssl;
        ctx = other.ctx;
        other.ssl = nullptr;
        other.ctx = nullptr;
    }

    impl & operator = (impl && other) noexcept
    {
        if (this != & other) {
            cleanup();
            ssl = other.ssl;
            ctx = other.ctx;
            other.ssl = nullptr;
            other.ctx = nullptr;
        }

        return *this;
    }

    ~impl ()
    {
        cleanup();
    }

    void cleanup ()
    {
        if (ssl != nullptr) {
            // It is acceptable for an application to call SSL_shutdown() once (such that it returns 0)
            // and then close the underlying connection without waiting for the peer's response.
            // This allows for a more rapid shutdown process if the application does not wish to
            // wait for the peer.
            // The fast shutdown approach can only be used if there is no intention to reuse the
            // underlying connection (e.g. a TCP connection) for further communication;
            // in this case, the full shutdown process must be performed to ensure synchronisation
            // SSL_shutdown() can be modified to set the connection to the "shutdown" state without
            // actually sending a close_notify alert message; see SSL_CTX_set_quiet_shutdown(3).
            // When "quiet shutdown" is enabled, SSL_shutdown() will always succeed and return 1
            // immediately.
            SSL_set_quiet_shutdown(ssl, 1);
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
 * Loads certificates and private keys.
 *
 * @return @c true on success or @c false otherwise.
 */
bool load_certificates (SSL_CTX * ctx, tls_options const & opts, error * perr);

} // namespace ssl

NETTY__NAMESPACE_END
