////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2026 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2026.04.21 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "tls_socket_impl.hpp"
#include "get_ssl_error.hpp"
#include <pfs/i18n.hpp>
#include <cstdint>

// #if _MSC_VER
// #   include <pfs/windows.hpp>
// #   include <wincrypt.h>
// #endif

NETTY__NAMESPACE_BEGIN

namespace ssl {

tls_socket::tls_socket () = default;

tls_socket::tls_socket (std::unique_ptr<tls_socket::impl> d)
    : _d(std::move(d))
{}

tls_socket::tls_socket (tls_socket && other) noexcept = default;
tls_socket & tls_socket::operator = (tls_socket && other) noexcept = default;
tls_socket::~tls_socket () = default;

tls_socket::operator bool () const noexcept
{
    return (_d && *_d);
}

tls_socket::socket_id tls_socket::id () const noexcept
{
    return _d->id();
}

socket4_addr tls_socket::saddr () const noexcept
{
    return _d->saddr();
}

conn_status tls_socket::connect (socket4_addr const & remote_saddr, tls_options opts, error * perr)
{
    _d = std::make_unique<impl>();
    _d->opts = std::move(opts);

    auto status = _d->connect(remote_saddr, perr);

    if (status == conn_status::failure)
        return status;

    SSL_METHOD const * method = TLS_client_method();

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // NOTE Below identical code as for tls_listener::listen

    if (method == nullptr) {
        pfs::throw_or(perr, make_error_code(errc::ssl_error), tr::_("TLS_server_method call failure"));
        return conn_status::failure;
    }

    SSL_CTX * ctx = SSL_CTX_new(method);

    if (ctx == nullptr) {
        pfs::throw_or(perr, make_error_code(errc::ssl_error), tr::_("SSL_CTX_new call failure"));
        return conn_status::failure;
    }

    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);

    long mode =
        // Once SSL_write_ex() or SSL_write() returns successful, r bytes have been written and the
        // next call to SSL_write_ex() or SSL_write() must only send the n-r bytes left, imitating the
        // behaviour of write()
          SSL_MODE_ENABLE_PARTIAL_WRITE

        // Make it possible to retry SSL_write_ex() or SSL_write() with changed buffer location
        // (the buffer contents must stay the same). This is not the default to avoid the
        // (misconception that nonblocking SSL_write() behaves like nonblocking write().
        | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER;

    SSL_CTX_set_mode(ctx, mode);

    std::uint64_t options =
        // Disable insecure protocols
          SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3

        // When choosing a cipher, signature, (TLS 1.2) curve or (TLS 1.3) group, use the server's
        // preferences instead of the client preferences. When not set, the SSL server will always
        // follow the clients preferences. When set, the SSL/TLS server will choose following its
        // own preferences.
        | SSL_OP_SERVER_PREFERENCE

        // Disable all renegotiation in (D)TLSv1.2 and earlier. Do not send HelloRequest messages,
        // and ignore renegotiation requests via ClientHello.
        | SSL_OP_NO_RENEGOTIATION;

    SSL_CTX_set_options(ctx, options);

    int encoding = _d->opts.format == encoding_format::pem
        ? SSL_FILETYPE_PEM
        : _d->opts.format == encoding_format::asn1
            ? SSL_FILETYPE_ASN1
            : SSL_FILETYPE_PEM;

    if (_d->opts.cert_file) {
        auto rc = SSL_CTX_use_certificate_file(ctx, _d->opts.cert_file->c_str(), encoding);

        if (rc != 1) {
            auto errn = ERR_get_error();
            pfs::throw_or(perr, get_ssl_error(errn
                , tr::f_("loading certificate from file (SSL_CTX_use_certificate_file) failure: {}"
                , *_d->opts.cert_file)));
            return conn_status::failure;
        }
    }

    if (_d->opts.key_file) {
        auto rc = SSL_CTX_use_PrivateKey_file(ctx, _d->opts.key_file->c_str(), encoding);

        if (rc != 1) {
            auto errn = ERR_get_error();
            pfs::throw_or(perr, get_ssl_error(errn
                , tr::f_("adding private key (SSL_CTX_use_PrivateKey_file) failure: {}"
                , *_d->opts.key_file)));
            return conn_status::failure;
        }

        // Check the consistency of a private key with the corresponding certificate loaded
        // into context.
        rc = SSL_CTX_check_private_key(ctx);

        if (rc != 1) {
            auto errn = ERR_get_error();
            pfs::throw_or(perr, get_ssl_error(errn
                , tr::f_("checking the consistency of a private key (SSL_CTX_check_private_key)"
                    " failure: {}", *_d->opts.key_file)));
            return conn_status::failure;
        }
    }
    //
    ////////////////////////////////////////////////////////////////////////////////////////////////

    auto ssl = SSL_new(ctx);

    if (ssl == nullptr) {
        auto errn = ERR_get_error();
        pfs::throw_or(perr, get_ssl_error(errn, tr::f_("creating data for connection (SSL_new) failure")));
        return conn_status::failure;
    }

    auto rc = SSL_set_fd(ssl, _d->id());

    if (rc == 0) {
        auto errn = ERR_get_error();
        pfs::throw_or(perr, get_ssl_error(errn, tr::_("association SSL with socket descriptor (SSL_set_fd) failure")));
        return conn_status::failure;
    }

    SSL_set_connect_state(ssl);

    rc = SSL_do_handshake(ssl);

    if (rc > 0) {
        status = conn_status::connected;
    } else {
        auto errn = SSL_get_error(ssl, rc);
        auto wait_required = (errn == SSL_ERROR_WANT_READ || errn == SSL_ERROR_WANT_WRITE);

        if (!wait_required) {
            pfs::throw_or(perr, get_ssl_error(errn, tr::_("handshake (SSL_do_handshake) failure")));
            return conn_status::failure;
        }
    }

    _d->ssl = ssl;
    _d->ctx = ctx;

    return status;
}

int tls_socket::recv (char * data, int len, error * perr)
{
    std::size_t bytes_read = 0;
    auto success = SSL_read_ex(_d->ssl, data, len, & bytes_read) > 0;

    if (!success) {
        auto errn = SSL_get_error(_d->ssl, 0);

        if (errn == SSL_ERROR_WANT_READ || errn == SSL_ERROR_WANT_WRITE) {
            return 0;
        } else {
            pfs::throw_or(perr, get_ssl_error(errn, tr::_("send (SSL_write_ex2) failure")));
            return -1;
        }
    }

    return bytes_read;
}

send_result tls_socket::send (char const * data, int len, error * perr)
{
    std::size_t bytes_written = 0;

    auto success = SSL_write_ex(_d->ssl, data, len, & bytes_written) > 0;

    if (!success) {
        auto errn = SSL_get_error(_d->ssl, 0);

        if (errn == SSL_ERROR_WANT_READ || errn == SSL_ERROR_WANT_WRITE) {
            return send_result{send_status::again, 0};
        } else {
            pfs::throw_or(perr, get_ssl_error(errn, tr::_("send (SSL_write_ex2) failure")));
            return send_result{send_status::failure, 0};
        }
    }

    return send_result{send_status::good, bytes_written};
}

} // namespace ssl

NETTY__NAMESPACE_END
