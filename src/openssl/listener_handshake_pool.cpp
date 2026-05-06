////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2026 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2026.04.27 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "ssl/listener_handshake_pool.hpp"
#include "get_ssl_error.hpp"
#include "tls_socket_impl.hpp"
#include <pfs/assert.hpp>
#include <pfs/i18n.hpp>

NETTY__NAMESPACE_BEGIN

namespace ssl {

listener_handshake_pool::listener_handshake_pool ()
    : basic_handshake_pool()
{
    reader_poller_t::on_failure = [this] (socket_id id, error const & err) {
        remove_later(id);
        this->on_failure(id, err);
    };

    reader_poller_t::on_disconnected = [this] (socket_id id) {
        remove_later(id);
        this->on_failure(id, error {make_error_code(errc::ssl_error), tr::_("socket disconnected while handshaking")});
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

        if (rc > 0) {
            remove_later(id);
            this->on_accepted(std::move(acc_ptr->sock));
        } else {
            auto errn = SSL_get_error(ssl, rc);
            auto wait_required = (errn == SSL_ERROR_WANT_READ || errn == SSL_ERROR_WANT_WRITE);

            if (wait_required)
                return;

            remove_later(id);
            auto err = get_ssl_error(errn, tr::_("handshake failure"));
            this->on_failure(id, err);
        }
    };
}

} // namespace ssl

NETTY__NAMESPACE_END
