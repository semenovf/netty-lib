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

tls_listener::tls_listener (socket4_addr const & saddr, tls_options opts, int backlog, error * perr)
    : _d(new impl(saddr, backlog, perr))
{
    _d->opts = std::move(opts);
}

    /**
     * Constructs TLS listener using option set in @a opts.
     *
     * @details @a opts may/must contain the following keys:
     *          * enc_format - encoding format, "pem" | "asn1" (default is "pem");
     *          * cert_file - certificate file path (mandatory for now);
     *          * key_file - private key file path (mandatory for now);
     */
tls_listener::tls_listener (std::map<std::string, std::string> const & opts, error * perr)
    : _d(new impl(opts, perr))
{
    tls_options tls_opts;
    tls_opts.format = encoding_format::pem;

    for (auto const & o: opts) {
        if (o.first == "enc_format") {
            if (o.second == "pem")
                tls_opts.format = encoding_format::pem;
            else if (o.second == "asn1")
                tls_opts.format = encoding_format::asn1;
            else {
                pfs::throw_or(perr, std::make_error_code(std::errc::invalid_argument)
                    , tr::f_("bad value for encoding format: {}", o.second));
                return;
            }
        } else if (o.first == "cert_file") {
            tls_opts.cert_file = o.second;
        } else if (o.first == "key_file") {
            tls_opts.key_file = o.second;
        }
    }

    if (tls_opts.cert_file) {
        pfs::throw_or(perr, std::make_error_code(std::errc::invalid_argument)
            , tr::_("certificate file path is mandatory"));
        return;
    }

    if (tls_opts.key_file) {
        pfs::throw_or(perr, std::make_error_code(std::errc::invalid_argument)
            , tr::_("private key file path is mandatory"));
        return;
    }

    _d->opts = std::move(tls_opts);
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
    auto success = _d->listen(perr);

    if (!success)
        return false;

    SSL_CTX * ctx = create_ssl_context(true, _d->opts, perr);

    if (ctx == nullptr)
        return false;

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
