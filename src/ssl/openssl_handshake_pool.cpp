////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2026 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2026.04.27 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "ssl/handshake_pool.hpp"
#include "tls_socket_impl.hpp"
#include <pfs/assert.hpp>
#include <pfs/i18n.hpp>

NETTY__NAMESPACE_BEGIN

namespace ssl {

handshake_pool::handshake_pool ()
    : reader_poller_t()
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

        if (rc == 1) {
            remove_later(id);
            this->on_accepted(std::move(acc_ptr->sock));
        } else {
            auto errn = SSL_get_error(ssl, rc);
            auto wait_required = (errn == SSL_ERROR_WANT_READ || errn == SSL_ERROR_WANT_WRITE);

            if (wait_required)
                return;

            remove_later(id);
            auto err = error {
                  make_error_code(errc::ssl_error)
                , tr::f_("handshake failure: {}", ERR_error_string(errn, nullptr))
            };

            this->on_failure(id, std::move(err));
        }
    };
}

void handshake_pool::add (socket_type && sock)
{
    auto id = sock.id();
    auto acc_ptr = locate_account(id);

    PFS__THROW_UNEXPECTED(acc_ptr == nullptr, "Fix the algorithm for a handshake pool:"
        " socket already exists in the pool when adding a new one with the same identifier.");

    if (acc_ptr == nullptr) {
        error err;

        reader_poller_t::add(id, & err);

        if (!err)
            _accounts.emplace(id, account{std::move(sock)});
        else
            this->on_failure(id, err);
    }
}

void handshake_pool::remove_later (socket_id id)
{
    _removed.push_back(id);
}

void handshake_pool::apply_remove ()
{
    if (!_removed.empty()) {
        for (auto id: _removed) {
            reader_poller_t::remove(id);
            _accounts.erase(id);
        }

        _removed.clear();
    }
}

unsigned int handshake_pool::step (error * perr)
{
    auto n = reader_poller_t::poll(std::chrono::milliseconds{0}, perr);
    return n > 0 ? static_cast<unsigned int>(n) : 0;
}

handshake_pool::account * handshake_pool::locate_account (socket_id id)
{
    auto pos = _accounts.find(id);

    if (pos == _accounts.end())
        return nullptr;

    auto & acc = pos->second;

    // Inconsistent data: requested socket ID is not equal to account's ID
    PFS__THROW_UNEXPECTED(acc.sock.id() == id, "Fix the algorithm for a reader pool");

    return & acc;
}

} // namespace ssl

NETTY__NAMESPACE_END
