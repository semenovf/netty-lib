////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2024-2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.12.31 Initial version.
//      2025.05.07 Replaced `std::function` with `callback_t`.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "namespace.hpp"
#include "archive.hpp"
#include "callback.hpp"
#include "error.hpp"
#include <pfs/assert.hpp>
#include <pfs/i18n.hpp>
#include <pfs/stopwatch.hpp>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <functional>
#include <unordered_map>
#include <vector>

NETTY__NAMESPACE_BEGIN

template <typename Socket, typename ReaderPoller, typename Archive>
class reader_pool: protected ReaderPoller
{
public:
    using archive_type = Archive;
    using socket_type = Socket;
    using socket_id = typename Socket::socket_id;

private:
    struct account
    {
        socket_id id {socket_type::kINVALID_SOCKET};
    };

private:
    std::unordered_map<socket_id, account> _accounts;
    std::vector<socket_id> _removed;
    std::uint16_t _chunk_size {1500}; // Initial value is default MTU size

public:
    mutable callback_t<void (socket_id, error const &)> on_failure = [] (socket_id, error const &) {};
    mutable callback_t<void (socket_id, archive_type)> on_data_ready;
    mutable callback_t<void (socket_id)> on_disconnected;
    mutable callback_t<Socket *(socket_id)> locate_socket = [] (socket_id) -> Socket * {
        PFS__TERMINATE(false, "socket location callback must be set");
        return nullptr;
    };

public:
    reader_pool (std::uint16_t chunk_size = 1500)
        : ReaderPoller()
        , _chunk_size(chunk_size)
    {
        ReaderPoller::on_failure = [this] (socket_id id, error const & err) {
            remove_later(id);
            this->on_failure(id, err);
        };

        ReaderPoller::on_disconnected = [this] (socket_id id) {
            if (this->on_disconnected)
                this->on_disconnected(id);
            remove_later(id);
        };

        ReaderPoller::on_ready_read = [this] (socket_id id) {
            auto acc = locate_account(id);

            // Inconsistent data: requested socket ID is not equal to account's ID
            PFS__THROW_UNEXPECTED(acc != nullptr, "Fix the algorithm of ready read for a reader pool:"
                " reader account not found by id");

            auto sock = this->locate_socket(id);

            if (sock == nullptr) {
                remove_later(id);
                this->on_failure(id, error {tr::f_("cannot locate socket for reading by ID: {}"
                    ", removed from reader pool", id)
                });
                return;
            }

            archive_type inpb;
            std::vector<char> chunk(_chunk_size);

            // Read all received data and put it into input buffer.
            for (;;) {
                error err;
                auto n = sock->recv(chunk.data(), chunk.size(), & err);

                if (n < 0) {
                    this->on_failure(id, err);
                    remove_later(id);
                    return;
                }

                inpb.append(chunk.data(), n);

                if (n == 0)
                    break;
            }

            if (this->on_data_ready) {
                if (!inpb.empty())
                    this->on_data_ready(id, std::move(inpb));
            }
        };
    }

public:
    void set_chunk_size (std::uint16_t chunk_size) noexcept
    {
        _chunk_size = chunk_size;
    }

    void add (socket_id id)
    {
        /*auto acc = */ensure_account(id);
    }

    void remove_later (socket_id id)
    {
        _removed.push_back(id);
    }

    void apply_remove ()
    {
        if (!_removed.empty()) {
            for (auto id: _removed) {
                ReaderPoller::remove(id);
                _accounts.erase(id);
            }

            _removed.clear();
        }
    }

    /**
     * @return Number of events occurred.
     */
    unsigned int step (error * perr = nullptr)
    {
        auto n = ReaderPoller::poll(std::chrono::milliseconds{0}, perr);
        return n > 0 ? static_cast<unsigned int>(n) : 0;
    }

private:
    account * locate_account (socket_id id)
    {
        auto pos = _accounts.find(id);

        if (pos == _accounts.end())
            return nullptr;

        auto & acc = pos->second;

        // Inconsistent data: requested socket ID is not equal to account's ID
        PFS__THROW_UNEXPECTED(acc.id == id, "Fix the algorithm for a reader pool");

        return & acc;
    }

    account * ensure_account (socket_id id)
    {
        auto acc = locate_account(id);

        if (acc == nullptr) {
            account a;
            a.id = id;
            auto res = _accounts.emplace(id, std::move(a));

            acc = & res.first->second;

            error err;
            ReaderPoller::add(id, & err);

            if (err)
                this->on_failure(id, err);
        }

        return acc;
    }
};

NETTY__NAMESPACE_END
