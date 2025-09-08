////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.01.20 Initial version.
//      2025.02.04 It is a part of patterns::meshnet now.
//      2025.09.08 Using chunk type.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include "../../chunk.hpp"
#include "priority_frame.hpp"
#include <pfs/assert.hpp>
#include <pfs/i18n.hpp>
#include <array>
#include <cstdint>
#include <queue>
#include <utility>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

template <typename PriorityTracker>
class priority_writer_queue
{
    using priority_tracker_type = PriorityTracker;
    using chunk_type = chunk;
    using chunk_queue_type = std::queue<chunk_type>;

    static constexpr std::size_t PRIORITY_COUNT = PriorityTracker::SIZE;

    static_assert(PRIORITY_COUNT > 0, "Priority count must be at least 1");

private:
    std::array<chunk_queue_type, PRIORITY_COUNT> _qpool; // Chunks queue pool
    std::vector<char> _frame; // Current sending frame
    bool _empty {true}; // Used for optimization
    priority_tracker_type _priority_tracker;

public:
    priority_writer_queue ()
    {}

private:
    // Result priority points to non-empty queue (after constructor or `shift` method calling).
    // A result of -1 indicates that all elements of the pool are empty
    int next_priority ()
    {
        // !empty() already checked in acquire_frame() before calling this method.

        auto priority = _priority_tracker.next();
        auto pq = & _qpool.at(priority);
        auto initial_pq = pq;
        int loops = 0;

        while (pq->empty()) {
            priority = _priority_tracker.skip();
            pq = & _qpool.at(priority);

            PFS__THROW_UNEXPECTED(loops <= PRIORITY_COUNT
                , tr::f_("Fix meshnet::priority_writer_queue algorithm: loops({}) > PRIORITY_COUNT({})"
                    , loops, PRIORITY_COUNT));

            // The cycle is complete
            if (pq == initial_pq)
                break;
        }

        if (pq->empty()) {
            PFS__THROW_UNEXPECTED(pq == initial_pq
                , tr::_("Fix meshnet::priority_writer_queue algorithm"));

            for (auto const & q: _qpool) {
                PFS__THROW_UNEXPECTED(q.empty(), tr::_("Fix meshnet::priority_writer_queue algorithm"));
            }

            priority = -1;
        }

        return priority;
    }

public:
    void enqueue (int priority, char const * data, std::size_t len)
    {
        if (len == 0)
            return;

        if (priority >= PRIORITY_COUNT)
            priority = PRIORITY_COUNT - 1;

        auto & q = _qpool.at(priority);

        q.push(chunk_type{data, data + len});
        _empty = false;
    }

    void enqueue (int priority, std::vector<char> && data)
    {
        if (data.empty())
            return;

        if (priority >= PRIORITY_COUNT)
            priority = PRIORITY_COUNT - 1;

        auto & q = _qpool.at(priority);

        q.push(chunk_type{std::move(data)});
        _empty = false;
    }

    std::vector<char> const & acquire_frame (std::size_t frame_size)
    {
        if (!_frame.empty()) {
            PFS__THROW_UNEXPECTED(_frame.size() <= frame_size, "");
            return _frame;
        }

        if (_empty)
            return _frame; // _frame is empty here

        auto priority = next_priority();

        if (priority < 0) {
            _empty = true;
            return _frame; // _frame is empty here
        }

        auto & q = _qpool.at(priority);
        auto & front = q.front();

        PFS__THROW_UNEXPECTED(_frame.empty(), "");

        priority_frame{}.pack(priority, _frame, front, frame_size);

        // Check topmost message is processed
        if (front.empty())
            q.pop();

        return _frame;
    }

    void shift (std::size_t n)
    {
        PFS__THROW_UNEXPECTED(n > 0, "");
        PFS__THROW_UNEXPECTED(n <= _frame.size(), "");

        if (_frame.size() == n)
            _frame.clear();
        else
            _frame.erase(_frame.begin(), _frame.begin() + n);
    }

public: // static
    static constexpr int priority_count () noexcept
    {
        return PRIORITY_COUNT;
    }
};

template <typename PriorityTracker>
constexpr std::size_t priority_writer_queue<PriorityTracker>::PRIORITY_COUNT;

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
