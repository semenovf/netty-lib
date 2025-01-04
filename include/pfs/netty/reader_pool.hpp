////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.12.31 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "error.hpp"
#include "namespace.hpp"
#include <pfs/assert.hpp>
#include <pfs/stopwatch.hpp>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <functional>
#include <unordered_map>
#include <vector>

NETTY__NAMESPACE_BEGIN

template <typename ReaderPoller, typename Socket>
class reader_pool: protected ReaderPoller
{
public:
    using socket_type = Socket;
    using socket_id = typename Socket::socket_id;

private:
    struct account
    {
        socket_id id;
        std::uint16_t chunk_size {1500}; // Initial value is default MTU size
    };

private:
    std::unordered_map<socket_id, account> _accounts;
    std::vector<socket_id> _removable;

    mutable std::function<void(socket_id, error const &)> _on_failure
        = [] (socket_id, error const &) {};
    mutable std::function<void(socket_id, std::vector<char> &&)> _on_ready
        = [] (socket_id, std::vector<char> &&) {};
    mutable std::function<void(socket_id)> _on_disconnected
        = [] (socket_id) {};

public:
    reader_pool (): ReaderPoller()
    {}

private:
    account * locate_account (socket_id id)
    {
        auto pos = _accounts.find(id);

        if (pos == _accounts.end())
            return nullptr;

        auto & acc = pos->second;

        // Inconsistent data: requested socket ID is not equal to account's ID
        PFS__TERMINATE(acc.id == id, "Fix the algorithm for a reader pool");

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
                _on_failure(id, err);
        }

        return acc;
    }

    void remove_later (socket_id id)
    {
        _removable.push_back(id);
    }

    void apply_remove ()
    {
        for (auto id: _removable) {
            ReaderPoller::remove(id);
            _accounts.erase(id);
        }

        _removable.clear();
    }

public:
    void add (socket_id id)
    {
        /*auto acc = */ensure_account(id);
    }

    void remove (socket_id id)
    {
        remove_later(id);
    }

    /**
     * Sets a callback for the failure. Callback signature is void(socket_id, netty::error const &).
     */
    template <typename F>
    reader_pool & on_failure (F && f)
    {
        _on_failure = std::forward<F>(f);

        ReaderPoller::on_failure = [this] (socket_id id, error const & err) {
            remove_later(id);
            _on_failure(id, err);
        };

        return *this;
    }

    /**
     * Sets a callback for reading from the socket. Callback signature is void(socket_id, std::vector<char> &&).
     */
    template <typename F>
    reader_pool & on_ready (F && f)
    {
        _on_ready = std::forward<F>(f);

        ReaderPoller::ready_read = [this] (socket_id id) {
            auto acc = locate_account(id);

            // Inconsistent data: requested socket ID is not equal to account's ID
            PFS__TERMINATE(acc != nullptr, "Fix the algorithm of ready read for a reader pool:"
                " reader account not found by id");

            std::vector<char> inpb;

            // Read all received data and put it into input buffer.
            for (;;) {
                error err;
                auto offset = inpb.size();
                inpb.resize(offset + acc->chunk_size);

                auto n = Socket::recv(id, inpb.data() + offset, acc->chunk_size, & err);

                if (n < 0) {
                    _on_failure(id, err);
                    remove_later(id);
                    return;
                }

                inpb.resize(offset + n);

                if (n == 0)
                    break;
            }

            _on_ready(id, std::move(inpb));
        };

        return *this;
    }

    template <typename F>
    reader_pool & on_disconnected (F && f)
    {
        _on_disconnected = std::forward<F>(f);
        ReaderPoller::disconnected = [this] (socket_id id) {
            _on_disconnected(id);
            remove_later(id);
        };
        return *this;
    }

    /**
     * @resturn Number of sockets ready for reading.
     */
    int step (std::chrono::milliseconds millis = std::chrono::milliseconds{0}, error * perr = nullptr)
    {
        pfs::stopwatch<std::milli> stopwatch;

        if (!_removable.empty())
            apply_remove();

        millis -= std::chrono::milliseconds{stopwatch.current_count()};
        return ReaderPoller::poll(millis, perr);
    }
};

NETTY__NAMESPACE_END
