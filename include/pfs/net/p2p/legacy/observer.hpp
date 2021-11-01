////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.09.20 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "utils.hpp"
#include "pfs/emitter.hpp"
#include "pfs/uuid.hpp"
#include "pfs/net/inet4_addr.hpp"
#include <chrono>
#include <map>
#include <mutex>
#include <utility>

namespace pfs {
namespace net {
namespace p2p {

class observer
{
    struct item
    {
        inet4_addr addr;
        std::uint16_t port;
        std::chrono::milliseconds expiration_timepoint;
    };

    std::map<uuid_t, item> _peers;
    std::chrono::milliseconds _nearest_expiration_timepoint {std::chrono::milliseconds::max()};

    std::mutex _mtx;

public: // signals
    /**
     * Emitted when new address accepted.
     */
    emitter_mt<uuid_t, inet4_addr const &, std::uint16_t> rookie_accepted;

    /**
     * Emitted when address expired (update is timed out).
     */
    emitter_mt<uuid_t, inet4_addr const &, std::uint16_t> expired;

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
    void update (uuid_t peer_uuid
        , inet4_addr const & addr
        , std::uint16_t port
        , std::chrono::milliseconds expiration_timeout)
    {
        std::unique_lock<std::mutex> locker(_mtx);

        auto now = current_timepoint();
        auto expiration_timepoint = now + expiration_timeout;

        auto it = _peers.find(peer_uuid);

        if (it == _peers.end()) {
            _peers.insert({peer_uuid, {addr, port, expiration_timepoint}});
            rookie_accepted(peer_uuid, addr, port);
        } else {
            it->second.expiration_timepoint = expiration_timepoint;
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

        for (auto it = _peers.cbegin(); it != _peers.cend();) {
            if (it->second.expiration_timepoint <= now) {
                auto addr = it->second.addr;
                auto port = it->second.port;
                expired(it->first, addr, port);

                it = _peers.erase(it);
            } else {
                if (expiration_timepoint > it->second.expiration_timepoint)
                    expiration_timepoint = it->second.expiration_timepoint;
                ++it;
            }
        }

        // Restart timer
        if (expiration_timepoint < std::chrono::milliseconds::max()) {
            assert(!_peers.empty());
            _nearest_expiration_timepoint = expiration_timepoint;
            nearest_expiration_changed(_nearest_expiration_timepoint);
        } else {
            assert(_peers.empty());
            _nearest_expiration_timepoint = std::chrono::milliseconds::max();
        }
    }
};

}}} // namespace pfs::net::p2p
