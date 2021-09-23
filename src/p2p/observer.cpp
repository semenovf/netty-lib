////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.09.20 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "pfs/net/p2p/observer.hpp"

namespace pfs {
namespace net {
namespace p2p {

void observer::update (inet4_addr const & addr
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

void observer::check_expiration ()
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

}}} // namespace pfs::net::p2p
