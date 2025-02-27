////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.02.17 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "serial_id.hpp"
#include "../../namespace.hpp"
#include <pfs/assert.hpp>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <deque>
#include <map>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace reliable_delivery {

/**
 * In-memory income messages processor
 */
class im_income_processor
{
    struct account
    {
        serial_id sid {0};
        std::vector<char> payload;

        account (serial_id sid, char const * data, std::size_t len)
            : sid(sid)
            , payload(data, data + len)
        {}
    };

private:
    // Bounds for sliding window
    //
    // last committed serial ID (committed_sid)
    //           |
    //           |      Cache
    //           | |<--------->|
    //           v |           |
    // +--+--+--+--+--+--+--+--+--+--+--+--+--+
    // |CC|CC|CC|CC|pp|  |pp|pp|  |  |  |  |  |
    // +--+--+--+--+--+--+--+--+--+--+--+--+--+
    //                       ^
    //                       |
    //    last income message serial ID (recent_sid)
    //

    serial_id _committed_sid {0}; // Last committed income message serial ID
    serial_id _recent_sid {0};    // Last income message serial ID

    std::deque<account> _cache;

public:
    /**
     * Constucts in-memory income messages processor using initial serial ID @a initial.
     * Initial serial ID must be the committed value from the previous session or zero,
     * if it is a first session for the processor.
     */
    im_income_processor (serial_id initial)
        : _committed_sid(initial)
        , _recent_sid(initial)
    {}

public:
    bool payload_expected (serial_id sid) const noexcept
    {
        return sid == _committed_sid + 1;
    }

    bool payload_duplicated (serial_id sid) const noexcept
    {
        return sid <= _committed_sid;
    }

    void commit (/*[[maybe_unused]]*/ serial_id sid)
    {
        (void)sid;
        ++_committed_sid;
        PFS__ASSERT(sid == _committed_sid, "");
    }

    void cache (serial_id sid, char const * data, std::size_t len)
    {
        PFS__ASSERT(sid > _committed_sid, "");
        auto & acc = _cache[sid - _committed_sid];
        acc.sid = sid;
        acc.payload = std::vector<char>(data, data + len);
    }

    void cache (serial_id sid, std::vector<char> && data)
    {
        PFS__ASSERT(sid > _committed_sid, "");
        auto & acc = _cache[sid - _committed_sid];
        acc.sid = sid;
        acc.payload = std::move(data);
    }

    // FIXME
    std::vector<serial_id> missed (serial_id last_sid) const noexcept
    {
        auto count = last_sid - _committed_sid;
        std::vector<serial_id> result;
        result.reserve(count);

        for (serial_id sid = _committed_sid + 1, index = 0; sid <= last_sid; sid++, index++) {
            // Skip already received messages
            if (_cache.size() > index && !_cache[index].payload.empty())
                continue;

            result.push_back(sid);
        }

        return result;
    }
};

/**
 * In-memory outcome messages processor
 */
class im_outcome_processor
{
    using clock_type = std::chrono::steady_clock;
    using time_point_type = clock_type::time_point;

    struct account
    {
        serial_id sid;
        time_point_type exp_time;
        std::vector<char> payload;

        account (serial_id sid, char const * data, std::size_t len, time_point_type exp_time)
            : sid(sid)
            , exp_time(exp_time)
            , payload(data, data + len)
        {}
    };

private:
    // Bounds for sliding window
    //
    // last acknowledged serial ID (ack_sid)
    //           |
    //           |   Cache
    //           | |<------>|
    //           v |        |
    // +--+--+--+--+--+--+--+--+--+--+--+--+--+
    // |AA|AA|AA|AA|pp|pp|pp|  |  |  |  |  |  |
    // +--+--+--+--+--+--+--+--+--+--+--+--+--+
    //                    ^
    //                    |
    //    last outcome message serial ID (recent_sid)
    //
    serial_id _ack_sid {0};    // Last acknowledged serial ID
    serial_id _recent_sid {0}; // Last outcome message serial ID

    time_point_type _oldest_exp_time;              // Oldest expiration time point
    std::chrono::milliseconds _exp_timeout {1000}; // Expiration timeout

    // Cache for outcome payloads (need access to random element)
    std::deque<account> _cache;

public:
    /**
     * Constucts in-memory outcome messages processor using initial serial ID @a initial.
     * Initial serial ID must be the committed value from the previous session or zero,
     * if it is a first session for the processor.
     */
    im_outcome_processor (serial_id initial_ack_sid, serial_id initial_recent_sid
        , std::chrono::milliseconds exp_timeout = std::chrono::milliseconds{1000})
        : _ack_sid(initial_ack_sid)
        , _recent_sid(initial_recent_sid)
        , _oldest_exp_time(clock_type::now() + exp_timeout)
        , _exp_timeout(exp_timeout)
    {}

public:
    serial_id next_serial ()
    {
        return ++_recent_sid;
    }

    /**
     * Push data back to cache
     */
    void cache (serial_id sid, char const * data, std::size_t len)
    {
        PFS__ASSERT(_recent_sid > 0 && _cache.size() == _recent_sid - _ack_sid - 1, "");
        _cache.emplace_back(sid, data, len, clock_type::now() + _exp_timeout);
    }

    void ack (serial_id sid)
    {
        PFS__ASSERT(sid == _ack_sid + 1, "");
        _cache.erase(_cache.begin());
        ++_ack_sid;
        PFS__ASSERT(_recent_sid >= _ack_sid, "");
    }

    std::vector<char> payload (serial_id sid)
    {
        PFS__ASSERT(_ack_sid < _recent_sid, "");
        PFS__ASSERT(sid <= _recent_sid, "");
        PFS__ASSERT(sid > _ack_sid, "");

        auto index = sid - _ack_sid - 1;

        PFS__ASSERT(!_cache[index].payload.empty(), "");

        return _cache[index].payload;
    }

    bool has_waiting () const
    {
        return !_cache.empty();
    }

    /**
     * @param f Invokable with signature `void (std::vector<char>)`.
     */
    template <typename F>
    void foreach_waiting (F && f)
    {
        if (_cache.empty())
            return;

        auto now = clock_type::now();

        if (_oldest_exp_time > now)
            return;

        for (auto const & acc: _cache) {
            if (acc.exp_time < now) {
                _oldest_exp_time = (std::min)(_oldest_exp_time, acc.exp_time);
                f(acc.payload);
            }
        }
    }
};

}} // namespace patterns::reliable_delivery

NETTY__NAMESPACE_END
