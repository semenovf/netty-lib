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
#include "pfs/net/exports.hpp"
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

class PFS_NET_LIB_DLL_EXPORT observer final
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
    observer () {}
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
        , std::chrono::milliseconds expiration_timeout);

    /**
     * Check expiration
     */
    void check_expiration ();
};

}}} // namespace pfs::net::p2p
