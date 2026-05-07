////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2026 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2026.04.21 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "tls_socket_impl.hpp"
#include <pfs/i18n.hpp>
#include <cstdint>

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
    // _d->opts = std::move(opts);

    auto status = _d->connect(remote_saddr, perr);

    if (status == conn_status::failure)
        return status;

    SSL_CTX * ctx = create_ssl_context(false, opts, perr);

    if (ctx == nullptr)
        return conn_status::failure;

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
