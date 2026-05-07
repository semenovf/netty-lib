////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2026 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2026.05.07 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "tls_socket_impl.hpp"
#include <pfs/i18n.hpp>

// #if _MSC_VER
// #   include <pfs/windows.hpp>
// #   include <wincrypt.h>
// #endif

NETTY__NAMESPACE_BEGIN

namespace ssl {

SSL_CTX * create_ssl_context (bool is_listener, tls_options const & opts, error * perr)
{
    SSL_METHOD const * method = is_listener
        ? TLS_server_method()
        : TLS_client_method();

    if (method == nullptr) {
        pfs::throw_or(perr, make_error_code(errc::ssl_error), tr::f_("{} call failure"
            , (is_listener ? "TLS_server_method()" : "TLS_client_method()")));
        return nullptr;
    }

    SSL_CTX * ctx = SSL_CTX_new(method);

    if (ctx == nullptr) {
        pfs::throw_or(perr, make_error_code(errc::ssl_error), tr::_("SSL_CTX_new call failure"));
        return nullptr;
    }

    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);

    //Set options for socket only
    if (!is_listener) {
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
    }

    int encoding = opts.format == encoding_format::pem
        ? SSL_FILETYPE_PEM
        : opts.format == encoding_format::asn1
            ? SSL_FILETYPE_ASN1
            : SSL_FILETYPE_PEM;

    if (opts.cert_file) {
        auto rc = SSL_CTX_use_certificate_file(ctx, opts.cert_file->c_str(), encoding);

        if (rc != 1) {
            auto errn = ERR_get_error();
            pfs::throw_or(perr, get_ssl_error(errn
                , tr::f_("loading certificate from file (SSL_CTX_use_certificate_file) failure: {}"
                , *opts.cert_file)));
            return nullptr;
        }
    }

    if (opts.key_file) {
        auto rc = SSL_CTX_use_PrivateKey_file(ctx, opts.key_file->c_str(), encoding);

        if (rc != 1) {
            auto errn = ERR_get_error();
            pfs::throw_or(perr, get_ssl_error(errn
                , tr::f_("adding private key (SSL_CTX_use_PrivateKey_file) failure: {}"
                , *opts.key_file)));
            return nullptr;
        }

        // Check the consistency of a private key with the corresponding certificate loaded
        // into context.
        rc = SSL_CTX_check_private_key(ctx);

        if (rc != 1) {
            auto errn = ERR_get_error();
            pfs::throw_or(perr, get_ssl_error(errn
                , tr::f_("checking the consistency of a private key (SSL_CTX_check_private_key)"
                    " failure: {}", *opts.key_file)));
            return nullptr;
        }
    }

    return ctx;
}

} // namespace ssl

NETTY__NAMESPACE_END
