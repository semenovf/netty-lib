////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2026 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2026.04.29 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "ssl/client_handshake_pool.hpp"
#include "tls_socket_impl.hpp"
#include "trace.hpp"
#include <pfs/assert.hpp>
#include <pfs/i18n.hpp>

NETTY__NAMESPACE_BEGIN

namespace ssl {

client_handshake_pool::client_handshake_pool ()
    : basic_handshake_pool()
{
    reader_poller_t::on_failure = [this] (socket_id id, error const & err) {
        remove_later(id);
        this->on_failure(id, err);
    };

    reader_poller_t::on_disconnected = [this] (socket_id id) {
        remove_later(id);
        this->on_failure(id, error {
              make_error_code(errc::ssl_error)
            , tr::_("socket disconnected while handshaking")
        });
    };

    reader_poller_t::on_ready_read = [this] (socket_id id) {
        auto acc_ptr = locate_account(id);

        // Inconsistent data: requested socket ID is not equal to account's ID
        PFS__THROW_UNEXPECTED(acc_ptr != nullptr, "Fix the algorithm of ready read for a handshake pool:"
            " socket not found by id");

        auto ssl = acc_ptr->sock._d->ssl;

        PFS__THROW_UNEXPECTED(ssl != nullptr, "Fix the algorithm of ready read for a handshake pool:"
            " pointer to SSL structure is null");

        auto rc = SSL_do_handshake(ssl);

        if (rc == 1) {
            remove_later(id);
            NETTY__TRACE("SSL", "Client connected:\n{}", acc_ptr->sock._d->dump_cipher());
            this->on_connected(std::move(acc_ptr->sock));
        } else {
            auto errn = SSL_get_error(ssl, rc);
            auto wait_required = (errn == SSL_ERROR_WANT_READ || errn == SSL_ERROR_WANT_WRITE);

            if (wait_required)
                return;

            remove_later(id);
            this->on_failure(id, get_ssl_error(errn, "handshake failure"));
        }
    };
}

} // namespace ssl

NETTY__NAMESPACE_END
