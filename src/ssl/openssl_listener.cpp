////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2026 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2026.04.24 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "tls_socket_impl.hpp"
#include "ssl/tls_listener.hpp"
#include "posix/tcp_listener.hpp"
#include <pfs/i18n.hpp>
#include <openssl/ssl.h>
#include <openssl/err.h>
// #include <atomic>
// #include <mutex>
//
// #if _MSC_VER
// #   include <pfs/windows.hpp>
// #   include <wincrypt.h>
// #endif

NETTY__NAMESPACE_BEGIN

namespace ssl {

struct tls_listener::impl: public posix::tcp_listener
{
    SSL_CTX * ctx {nullptr};
    tls_options opts;

    using posix::tcp_listener::tcp_listener;

    ~impl ()
    {
        if (ctx != nullptr) {
            SSL_CTX_free(ctx);
            ctx = nullptr;
        }
    }
};

tls_listener::tls_listener () = default;
tls_listener::tls_listener (tls_listener &&) noexcept = default;

tls_listener::tls_listener (socket4_addr const & saddr, tls_options opts, error * perr)
    : _d(new impl(saddr, perr))
{
    _d->opts = std::move(opts);
}

tls_listener::~tls_listener () = default;

tls_listener & tls_listener::operator = (tls_listener && other) noexcept = default;

tls_listener::operator bool () const noexcept
{
    return (_d && *_d);
}

tls_listener::listener_id tls_listener::id () const noexcept
{
    return _d->id();
}

bool tls_listener::listen (int backlog, error * perr)
{
    auto success = _d->listen(backlog, perr);

    if (!success)
        return false;

    SSL_METHOD const * method = TLS_server_method();

    if (method == nullptr) {
        pfs::throw_or(perr, make_error_code(errc::ssl_error), tr::_("TLS_server_method call failure"));
        return false;
    }

    SSL_CTX * ctx = SSL_CTX_new(method);

    if (ctx == nullptr) {
        pfs::throw_or(perr, make_error_code(errc::ssl_error), tr::_("SSL_CTX_new call failure"));
        return false;
    }

    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);

    int encoding = _d->opts.format == encoding_format::pem
        ? SSL_FILETYPE_PEM
        : _d->opts.format == encoding_format::asn1
            ? SSL_FILETYPE_ASN1
            : SSL_FILETYPE_PEM;

    if (_d->opts.cert_file) {
        auto rc = SSL_CTX_use_certificate_file(ctx, _d->opts.cert_file->c_str(), encoding);

        if (rc != 1) {
            auto errn = ERR_get_error();
            pfs::throw_or(perr, make_error_code(errc::ssl_error)
                , tr::f_("loading certificate from file (SSL_CTX_use_certificate_file) failure: {}: {}"
                , *_d->opts.cert_file, ERR_error_string(errn, nullptr)));
            return false;
        }
    }

    if (_d->opts.key_file) {
        auto rc = SSL_CTX_use_PrivateKey_file(ctx, _d->opts.key_file->c_str(), encoding);

        if (rc != 1) {
            auto errn = ERR_get_error();
            pfs::throw_or(perr, make_error_code(errc::ssl_error)
                , tr::f_("adding private key (SSL_CTX_use_PrivateKey_file) failure: {}: {}"
                , *_d->opts.key_file, ERR_error_string(errn, nullptr)));
            return false;
        }

        // Check the consistency of a private key with the corresponding certificate loaded
        // into context.
        rc = SSL_CTX_check_private_key(ctx);

        if (rc != 1) {
            auto errn = ERR_get_error();
            pfs::throw_or(perr, make_error_code(errc::ssl_error)
                , tr::f_("checking the consistency of a private key (SSL_CTX_check_private_key)"
                    " failure: {}: {}"
                , *_d->opts.key_file, ERR_error_string(errn, nullptr)));
            return false;
        }
    }

    _d->ctx = ctx;

    return true;
}

tls_socket tls_listener::accept (bool force_nonblocking, error * perr)
{
    auto s = _d->accept(perr);

    if (!s)
        return tls_socket{};

    if (force_nonblocking) {
        if (!s.set_nonblocking(true, perr))
            return tls_socket{};
    }

    SSL * ssl = SSL_new(_d->ctx);

    if (ssl == nullptr) {
        auto errn = ERR_get_error();
        pfs::throw_or(perr, make_error_code(errc::ssl_error)
            , tr::f_("creating data for connection (SSL_new) failure: {}"
            , ERR_error_string(errn, nullptr)));
        return tls_socket{};
    }

    // Associate SSL with non-blocking fd
    auto rc = SSL_set_fd(ssl, s.id());

    if (rc == 0) {
        auto errn = ERR_get_error();
        pfs::throw_or(perr, make_error_code(errc::ssl_error)
            , tr::f_("association SSL with socket descriptor (SSL_set_fd) failure: {}"
            , ERR_error_string(errn, nullptr)));
        return tls_socket{};
    }

    SSL_set_accept_state(ssl);

    auto d = std::make_unique<tls_socket::impl>(std::move(s));
    d->ssl = ssl;

    return tls_socket{std::move(d)};
}

tls_socket tls_listener::accept (error * perr)
{
    return accept(false, perr);
}

tls_socket tls_listener::accept_nonblocking (error * perr)
{
    return accept(true, perr);
}

} // namespace ssl

NETTY__NAMESPACE_END
