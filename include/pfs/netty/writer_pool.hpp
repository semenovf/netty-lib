////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.12.27 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "error.hpp"
#include "namespace.hpp"
#include "send_result.hpp"
#include "writer_queue.hpp"
#include <pfs/assert.hpp>
#include <pfs/stopwatch.hpp>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <unordered_map>
#include <vector>

NETTY__NAMESPACE_BEGIN

template <typename Socket, typename WriterPoller, typename WriterQueue = writer_queue>
class writer_pool: protected WriterPoller
{
public:
    using socket_type = Socket;
    using socket_id = typename Socket::socket_id;

    static constexpr std::uint16_t default_frame_size ()
    {
        return 1500;
    }

private:
    struct account
    {
        socket_id id;
        bool writable {false};   // Socket is writable
        std::uint16_t frame_size {default_frame_size()}; // Initial value is default MTU size
        WriterQueue q; // Output queue
    };

    struct item
    {
        socket_id id;
    };

private:
    std::uint64_t _remain_bytes {0};
    std::unordered_map<socket_id, account> _accounts;
    std::vector<socket_id> _removable;

    mutable std::function<void(socket_id, error const &)> _on_failure = [] (socket_id, error const &) {};
    mutable std::function<void(socket_id, std::uint64_t)> _on_bytes_written;
    mutable std::function<Socket *(socket_id)> _locate_socket = [] (socket_id) -> Socket * {
        PFS__TERMINATE(false, "socket location callback must be set");
        return nullptr;
    };

public:
    writer_pool (): WriterPoller()
    {
        WriterPoller::on_failure = [this] (socket_id id, error const & err) {
            remove_later(id);
            _on_failure(id, err);
        };

        WriterPoller::can_write = [this] (socket_id id) {
            auto acc = locate_account(id);

            if (acc != nullptr)
                acc->writable = true;
        };
    }

private:
    account * locate_account (socket_id id)
    {
        auto pos = _accounts.find(id);

        if (pos == _accounts.end())
            return nullptr;

        auto & acc = pos->second;

        // Inconsistent data: requested socket ID is not equal to account's ID
        PFS__TERMINATE(acc.id == id, "Fix the algorithm for a writer pool");

        return & acc;
    }

    account * ensure_account (socket_id id, std::uint16_t frame_size = default_frame_size())
    {
        auto acc = locate_account(id);

        if (acc == nullptr) {
            account a;
            a.id = id;
            a.frame_size = frame_size;
            auto res = _accounts.emplace(id, std::move(a));

            acc = & res.first->second;
            WriterPoller::wait_for_write(acc->id);
        }

        return acc;
    }

    void send (std::chrono::milliseconds limit = std::chrono::milliseconds{0}, error * perr = nullptr)
    {
        pfs::stopwatch<std::milli> stopwatch;

        do {
            for (auto & item: _accounts) {
                auto & acc = item.second;

                if (!acc.writable)
                    continue;

                if (acc.q.empty())
                    continue;

                auto sock = _locate_socket(acc.id);

                if (sock == nullptr) {
                    remove_later(acc.id);
                    _on_failure(acc.id, error {errc::device_not_found, tr::f_("cannot locate socket for writing by ID: {}"
                        ", removed from writer pool", acc.id)});
                    continue;
                }

                auto frame = acc.q.frame(acc.frame_size);

                if (frame.empty())
                    continue;

                netty::error err;
                auto res = sock->send(frame.data(), frame.size(), & err);

                switch (res.status) {
                    case netty::send_status::failure:
                    case netty::send_status::network:
                        remove_later(acc.id);
                        _on_failure(acc.id, err);
                        break;

                    case netty::send_status::again:
                    case netty::send_status::overflow:
                        if (acc.writable) {
                            acc.writable = false;
                            WriterPoller::wait_for_write(acc.id);
                        }
                        break;

                    case netty::send_status::good:
                        if (res.n > 0) {
                            _remain_bytes -= res.n;
                            acc.q.shift(res.n);

                            if (_on_bytes_written)
                                _on_bytes_written(acc.id, res.n);
                        }
                        break;
                }
            }
        } while (stopwatch.current_count() < limit.count());
    }

public:
    void add (socket_id id, std::uint16_t frame_size = default_frame_size())
    {
        /*auto acc = */ensure_account(id, frame_size);
    }

    void remove_later (socket_id id)
    {
        _removable.push_back(id);
    }

    void apply_remove ()
    {
        if (!_removable.empty()) {
            for (auto id: _removable) {
                WriterPoller::remove(id);
                _accounts.erase(id);
            }

            _removable.clear();
        }
    }

    std::uint64_t remain_bytes () const noexcept
    {
        return _remain_bytes;
    }

    void enqueue (socket_id id, int priority, char const * data, std::size_t len)
    {
        if (len == 0)
            return;

        auto acc = ensure_account(id);
        acc->q.enqueue(priority, data, len);
        _remain_bytes += len;
    }

    void enqueue (socket_id id, char const * data, std::size_t len)
    {
        enqueue(id, 0, data, len);
    }

    void enqueue (socket_id id, int priority, std::vector<char> && data)
    {
        if (data.empty())
            return;

        auto acc = ensure_account(id);
        _remain_bytes += data.size();
        acc->q.enqueue(priority, std::move(data));
    }

    void enqueue (socket_id id, std::vector<char> && data)
    {
        enqueue(id, 0, std::move(data));
    }

    /**
     * Sets a callback for the failure. Callback signature is void(socket_id, netty::error const &).
     */
    template <typename F>
    writer_pool & on_failure (F && f)
    {
        _on_failure = std::forward<F>(f);
        return *this;
    }

    template <typename F>
    writer_pool & on_bytes_written (F && f)
    {
        _on_bytes_written = std::forward<F>(f);
        return *this;
    }

    template <typename F>
    writer_pool & on_locate_socket (F && f)
    {
        _locate_socket = std::forward<F>(f);
        return *this;
    }

    /**
     * @resturn Number of sockets waiting for writing.
     */
    void step (std::chrono::milliseconds millis = std::chrono::milliseconds{0}, error * perr = nullptr)
    {
        pfs::stopwatch<std::milli> stopwatch;

        millis -= std::chrono::milliseconds{stopwatch.current_count()};
        stopwatch.start();
        send(millis, perr);
        millis -= std::chrono::milliseconds{stopwatch.current_count()};

        WriterPoller::poll(millis, perr);
    }

    bool empty () const noexcept
    {
        return _accounts.empty();
    }

public: // static
    static constexpr int priority_count () noexcept
    {
        return WriterQueue::priority_count();
    }
};

NETTY__NAMESPACE_END
