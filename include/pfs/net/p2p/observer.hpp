////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.09.20 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/emitter.hpp"
#include "pfs/net/inet4_addr.hpp"
#include <chrono>
#include <map>
#include <mutex>
#include <utility>

namespace pfs {
namespace net {
namespace p2p {

inline std::chrono::milliseconds current_timepoint ()
{
    using std::chrono::duration_cast;
    using std::chrono::milliseconds;
    using std::chrono::steady_clock;

    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch());
}

class observer
{
    using key_type = std::pair<inet4_addr, std::uint16_t>;

    std::map<key_type, std::chrono::milliseconds> _services;
    std::chrono::milliseconds _nearest_expiration_timepoint {std::chrono::milliseconds::max()};

    std::mutex _mtx;

public: // signals
    /**
     * Emitted when new address accepted.
     */
    emitter_mt<inet4_addr const &, std::uint16_t> rookie_accepted;

    /**
     * Emitted when address expired (update is timed out).
     */
    emitter_mt<inet4_addr const &, std::uint16_t> expired;

    /**
     * Emitted when nearest expiration time point is changed.
     * Should be used for restart expiration timer.
     */
    emitter_mt<std::chrono::milliseconds> nearest_expiration_changed;

public:
    observer () = default;
    ~observer () = default;

    observer (observer const &) = delete;
    observer & operator = (observer const &) = delete;
    observer (observer &&) = delete;
    observer & operator = (observer &&) = delete;

    /**
     * Update peer
     */
    void update (inet4_addr const & addr
        , std::uint16_t port
        , std::chrono::milliseconds expiration_timeout)
    {
        std::unique_lock<std::mutex> locker(_mtx);

        auto now = current_timepoint();
        auto expiration_timepoint = now + expiration_timeout;

        auto key = std::make_pair(addr, port);
        auto it = _services.find(key);

        if (it == _services.end()) {
            _services.insert({key, expiration_timepoint});
            rookie_accepted(addr, port);
        } else {
            it->second = expiration_timepoint;
        }

        if (_nearest_expiration_timepoint > expiration_timepoint) {
            _nearest_expiration_timepoint = expiration_timepoint;
            nearest_expiration_changed(_nearest_expiration_timepoint);
        }
    }

    /**
     * Check expiration
     */
    void check_expiration ()
    {
        std::unique_lock<std::mutex> locker(_mtx);

        auto now = current_timepoint();
        auto expiration_timepoint = std::chrono::milliseconds::max();

        for (auto it = _services.cbegin(); it != _services.cend();) {
            if (it->second <= now) {
                auto addr = it->first.first;
                auto port = it->first.second;
                expired(addr, port);

                it = _services.erase(it);
            } else {
                if (expiration_timepoint > it->second)
                    expiration_timepoint = it->second;
                ++it;
            }
        }

        // Restart timer
        if (expiration_timepoint < std::chrono::milliseconds::max()) {
            assert(!_services.empty());
            _nearest_expiration_timepoint = expiration_timepoint;
            nearest_expiration_changed(_nearest_expiration_timepoint);
        } else {
            assert(_services.empty());
            _nearest_expiration_timepoint = std::chrono::milliseconds::max();
        }
    }
};

}}} // namespace pfs::net::p2p
