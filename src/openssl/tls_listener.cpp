////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2026 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2026.04.24 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "ssl/tls_listener.hpp"
#include "tls_socket_impl.hpp"
#include "posix/tcp_listener.hpp"
#include <pfs/i18n.hpp>

NETTY__NAMESPACE_BEGIN

namespace ssl {

struct tls_listener::impl: public posix::tcp_listener
{
    SSL_CTX * ctx {nullptr};

    impl (): posix::tcp_listener() {}
    impl (posix::tcp_listener && tl) noexcept: posix::tcp_listener(std::move(tl)) {}
    impl (listener_options const & opts, error * perr): posix::tcp_listener(opts, perr) {}

    impl (impl && other) noexcept: posix::tcp_listener(std::move(other))
    {
        ctx = other.ctx;
        other.ctx = nullptr;
    }

    impl & operator = (impl && other) noexcept
    {
        if (this != & other) {
            cleanup();
            ctx = other.ctx;
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
        if (ctx != nullptr) {
            SSL_CTX_free(ctx);
            ctx = nullptr;
        }
    }
};

tls_listener::tls_listener () = default;
tls_listener::tls_listener (tls_listener &&) noexcept = default;

tls_listener::tls_listener (listener_options const & opts, error * perr)
    : _d(new impl(opts, perr))
{
    SSL_METHOD const * method = TLS_server_method();

    if (method == nullptr) {
        pfs::throw_or(perr, make_error_code(errc::ssl_error), tr::_("TLS_server_method() call failure"));
        return;
    }

    SSL_CTX * ctx = SSL_CTX_new(method);

    if (ctx == nullptr) {
        pfs::throw_or(perr, make_error_code(errc::ssl_error), tr::_("SSL_CTX_new call failure"));
        return;
    }

    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);

    auto success = load_certificates(ctx, opts.tls, perr);

    if (!success)
        return;

    _d->ctx = ctx;
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

bool tls_listener::listen (error * perr)
{
    return _d->listen(perr);
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
        pfs::throw_or(perr, get_ssl_error(errn, tr::_("creating data for connection (SSL_new) failure")));
        return tls_socket{};
    }

    // Associate SSL with non-blocking fd
    auto rc = SSL_set_fd(ssl, s.id());

    if (rc == 0) {
        auto errn = ERR_get_error();
        pfs::throw_or(perr, get_ssl_error(errn, tr::f_("association SSL with socket descriptor (SSL_set_fd) failure")));
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
