////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2024-2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.12.27 Initial version.
//      2025.05.07 Replaced `std::function` with `callback_t`.
//      2025.06.30 Method `ensure()` renamed to `set_frame_size()`.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "namespace.hpp"
#include "callback.hpp"
#include "error.hpp"
#include "send_result.hpp"
#include "tag.hpp"
#include "trace.hpp"
#include <pfs/assert.hpp>
#include <pfs/i18n.hpp>
#include <pfs/stopwatch.hpp>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <unordered_map>
#include <vector>

NETTY__NAMESPACE_BEGIN

enum class bandwidth_throttling
{
      unlimited
    , adaptive
    , custom
};

template <typename Socket, typename WriterPoller, typename WriterQueue>
class writer_pool: protected WriterPoller
{
    using clock_type = std::chrono::steady_clock;
    using time_point_type = clock_type::time_point;

public:
    using socket_type = Socket;
    using socket_id = typename Socket::socket_id;

    static constexpr std::uint16_t default_frame_size ()
    {
        return 1500;
    }

private:
    struct bandwidth_data
    {
        std::size_t recent_bytes_sent {0};
        time_point_type recent_time_point;
        std::uint64_t data_rate {(std::numeric_limits<std::uint64_t>::max)()};
        bandwidth_throttling throttling {bandwidth_throttling::adaptive};
    };

    struct account
    {
        socket_id sid {socket_type::kINVALID_SOCKET};
        std::uint16_t max_frame_size {default_frame_size()}; // Initial value is default MTU size
        WriterQueue q; // Output queue

        bool writable {false};   // Socket is writable
        time_point_type writable_time_point;
        std::uint16_t writable_counter {0};

        bandwidth_data bwd;
    };

    struct item
    {
        socket_id sid;
    };

private:
    std::unordered_map<socket_id, account> _accounts;
    std::vector<socket_id> _removable;
    bandwidth_throttling _throttling {bandwidth_throttling::adaptive};
    std::size_t _data_rate_limit = (std::numeric_limits<std::size_t>::max)();
    std::uint16_t (* _tune_frame_size) (writer_pool *, account &, std::uint16_t) {nullptr};

public:
    mutable callback_t<void (socket_id, error const &)> on_failure = [] (socket_id, error const &) {};
    mutable callback_t<void (socket_id)> on_disconnected = [] (socket_id) {};
    mutable callback_t<Socket *(socket_id)> locate_socket = [] (socket_id) -> Socket * {
        PFS__THROW_UNEXPECTED(false, "socket location callback must be set");
        return nullptr;
    };
    mutable callback_t<void (socket_id, std::size_t)> on_data_rate;

public:
    writer_pool (bandwidth_throttling bt = bandwidth_throttling::adaptive
        , std::size_t data_rate_limit = (std::numeric_limits<std::size_t>::max)())
        : _throttling(bt)
        , _data_rate_limit(data_rate_limit)
    {
        switch (_throttling) {
            case bandwidth_throttling::unlimited:
                _tune_frame_size = tune_frame_size_unlimited;
                break;
            case bandwidth_throttling::adaptive:
                _tune_frame_size = tune_frame_size_adaptive;
                break;
            case bandwidth_throttling::custom:
                _tune_frame_size = tune_frame_size_static;
                break;
        }

        WriterPoller::on_failure = [this] (socket_id sid, error const & err) {
            remove_later(sid);
            this->on_failure(sid, err);
        };

        WriterPoller::on_disconnected = [this] (socket_id sid) {
            remove_later(sid);
            this->on_disconnected(sid);
        };

        WriterPoller::can_write = [this] (socket_id sid) {
            auto acc = locate_account(sid);

            if (acc != nullptr) {
                acc->writable = true;

                // Do not immediately allow to write, delay writability.
                // Let more time to socket to "drain" output buffer and peer socket to process
                // input data.
                // TODO Replace "magic number" with the configurable value
                acc->writable_time_point = clock_type::now() + std::chrono::milliseconds{500};
            }
        };
    }

public:
    /**
     * Associates data rate with specified socket ID @a sid.
     */
    void set_max_rate (socket_id sid, std::size_t rate)
    {
        auto pacc = locate_account(sid);

        if (pacc != nullptr)
            pacc->data_rate = rate;
    }

    /**
     * Associates frame size with specified socket ID @a sid.
     */
    void set_frame_size (socket_id sid, std::uint16_t frame_size)
    {
        auto pacc = locate_account(sid);

        if (pacc != nullptr)
            pacc->max_frame_size = frame_size;
    }

    void remove_later (socket_id sid)
    {
        _removable.push_back(sid);
    }

    void apply_remove ()
    {
        if (!_removable.empty()) {
            for (auto sid: _removable) {
                WriterPoller::remove(sid);
                _accounts.erase(sid);
            }

            _removable.clear();
        }
    }

    void enqueue (socket_id sid, int priority, char const * data, std::size_t len)
    {
        check_priority(priority);

        if (len == 0)
            return;

        auto acc = ensure_account(sid);
        acc->q.enqueue(priority, data, len);
    }

    void enqueue (socket_id sid, char const * data, std::size_t len)
    {
        enqueue(sid, 0, data, len);
    }

    void enqueue (socket_id sid, int priority, std::vector<char> && data)
    {
        check_priority(priority);

        if (data.empty())
            return;

        auto acc = ensure_account(sid);
        acc->q.enqueue(priority, std::move(data));
    }

    void enqueue (socket_id sid, std::vector<char> && data)
    {
        enqueue(sid, 0, std::move(data));
    }

    void enqueue_broadcast (int priority, char const * data, std::size_t len)
    {
        for (auto & acc: _accounts)
            enqueue(acc.second.sid, priority, data, len);
    }

    void enqueue_broadcast (char const * data, std::size_t len)
    {
        enqueue_broadcast(0, data, len);
    }

    /**
     * @return >= 0 - number of events occurred,
     *          < 0 - error occurred.
     */
    unsigned int step (error * perr = nullptr)
    {
        auto result = 0;
        result += send(perr);
        auto n = WriterPoller::poll(std::chrono::milliseconds{0}, perr);

        result += n > 0 ? n : 0;

        return result;
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

private:
    void check_priority (int priority)
    {
        if (priority < 0 || priority >= WriterQueue::priority_count()) {
            throw error { make_error_code(std::errc::invalid_argument)
                , tr::f_("bad priority value: must be less than {}, got: {}"
                    , WriterQueue::priority_count(), priority)
            };
        }
    }

    account * locate_account (socket_id sid)
    {
        auto pos = _accounts.find(sid);

        if (pos == _accounts.end())
            return nullptr;

        auto & acc = pos->second;

        // Inconsistent data: requested socket ID is not equal to account's ID
        PFS__THROW_UNEXPECTED(acc.sid == sid, "Fix the algorithm for a writer pool");

        return & acc;
    }

    account * ensure_account (socket_id sid)
    {
        auto acc = locate_account(sid);

        if (acc == nullptr) {
            account a;
            a.sid = sid;
            a.max_frame_size = default_frame_size();
            a.writable_time_point = clock_type::now();
            a.bwd.recent_time_point = clock_type::now();
            a.bwd.throttling = _throttling;
            a.bwd.data_rate = _data_rate_limit;
            auto res = _accounts.emplace(sid, std::move(a));

            acc = & res.first->second;
            WriterPoller::wait_for_write(acc->sid);
        }

        return acc;
    }

    /**
     * @return Number of successful frame sendings.
     */
    unsigned int send (error * perr = nullptr)
    {
        unsigned int result = 0;

        do {
            for (auto & item: _accounts) {
                auto & acc = item.second;

                if (!acc.writable)
                    continue;

                if (clock_type::now() < acc.writable_time_point)
                    continue;

                auto frame_size = _tune_frame_size(this, acc, acc.max_frame_size);

                if (frame_size == 0)
                    continue;

                auto frame = acc.q.acquire_frame(frame_size);

                if (frame.empty())
                    continue;

                // A missing socket is less common than an empty frame, so optimally locate socket
                // after frame acquiring.
                auto sock = this->locate_socket(acc.sid);

                if (sock == nullptr) {
                    remove_later(acc.sid);
                    this->on_failure(acc.sid, error {
                        tr::f_("cannot locate socket for writing by socket ID: {}"
                            ", removing from writer pool", acc.sid)
                    });

                    continue;
                }

                error err;
                auto res = sock->send(frame.data(), frame.size(), & err);

                switch (res.status) {
                    case send_status::failure:
                        remove_later(acc.sid);
                        this->on_failure(acc.sid, err);
                        break;
                    case send_status::network:
                        remove_later(acc.sid);
                        this->on_failure(acc.sid, err);
                        break;

                    case send_status::again:
                    case send_status::overflow:
                        if (acc.writable) {
                            acc.writable = false;
                            acc.writable_counter++;
                            WriterPoller::wait_for_write(acc.sid);
                        }
                        break;

                    case send_status::good:
                        if (res.n > 0) {
                            acc.q.shift(res.n);
                            acc.bwd.recent_bytes_sent += res.n;
                            result++;
                        }

                        break;
                }
            }
        } while (false);

        return result;
    }

private: // static
    static std::uint16_t tune_frame_size_unlimited (writer_pool * caller, account & acc
        , std::uint16_t initial_size)
    {
        auto const elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            clock_type::now() - acc.bwd.recent_time_point);

        if (elapsed_ms > std::chrono::milliseconds{999}) {
            std::size_t rate = (static_cast<double>(acc.bwd.recent_bytes_sent) / elapsed_ms.count()) * 1000;

            if (caller->on_data_rate)
                caller->on_data_rate(acc.sid, rate);

            acc.bwd.recent_bytes_sent = 0;
            acc.bwd.recent_time_point = clock_type::now();
        }

        return initial_size;
    }

    static std::uint16_t tune_frame_size_adaptive (writer_pool * caller, account & acc
        , std::uint16_t initial_size)
    {
        if (acc.writable_counter > 0) {
            if (acc.bwd.data_rate >= 1024 * 1024 * 1024)
                acc.bwd.data_rate /= 10;
            else if (acc.bwd.data_rate >= 1024 * 1024)
                acc.bwd.data_rate /= 1.5;
            else if (acc.bwd.data_rate >= 1024)
                acc.bwd.data_rate /= 1.1;
            else if (acc.bwd.data_rate > 2)
                acc.bwd.data_rate /= 1.01;

            acc.writable_counter = 0;
        }

        return tune_frame_size_static(caller, acc, initial_size);
    }

    static std::uint16_t tune_frame_size_static (writer_pool * caller, account & acc
        , std::uint16_t initial_size)
    {
        auto const elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            clock_type::now() - acc.bwd.recent_time_point);

        if (elapsed_ms <= std::chrono::milliseconds{999}) {
            if (acc.bwd.recent_bytes_sent < acc.bwd.data_rate /* multiplied by 1 sec */) {
                // Remain bytes in 1 second period
                auto frame_size = static_cast<std::uint16_t>((std::min)(static_cast<std::size_t>(initial_size)
                    , acc.bwd.data_rate - acc.bwd.recent_bytes_sent));

                // Avoid use too small frames
                return (std::max)(frame_size, initial_size);
            } else {
                return 0;
            }
        } else {
            std::size_t rate = (static_cast<double>(acc.bwd.recent_bytes_sent) / elapsed_ms.count()) * 1000;

            if (caller->on_data_rate)
                caller->on_data_rate(acc.sid, rate);

            acc.bwd.recent_bytes_sent = 0;
            acc.bwd.recent_time_point = clock_type::now();
            return static_cast<std::uint16_t>((std::min)(static_cast<std::size_t>(initial_size)
                , acc.bwd.data_rate));
        }
    }
};

NETTY__NAMESPACE_END
