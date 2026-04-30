////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2026 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2026.04.30 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "ssl/basic_handshake_pool.hpp"
#include "openssl_socket_impl.hpp"
#include <pfs/assert.hpp>
#include <pfs/i18n.hpp>

NETTY__NAMESPACE_BEGIN

namespace ssl {

basic_handshake_pool::basic_handshake_pool () = default;

void basic_handshake_pool::add (socket_type && sock)
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

void basic_handshake_pool::remove_later (socket_id id)
{
    _removable.push_back(id);
}

void basic_handshake_pool::apply_remove ()
{
    if (!_removable.empty()) {
        for (auto id: _removable) {
            reader_poller_t::remove(id);
            _accounts.erase(id);
        }

        _removable.clear();
    }
}

unsigned int basic_handshake_pool::step (error * perr)
{
    auto n = reader_poller_t::poll(std::chrono::milliseconds{0}, perr);
    return n > 0 ? static_cast<unsigned int>(n) : 0;
}

basic_handshake_pool::account * basic_handshake_pool::locate_account (socket_id id)
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

