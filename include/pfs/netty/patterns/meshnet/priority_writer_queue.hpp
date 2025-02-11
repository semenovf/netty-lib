////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.01.20 Initial version.
//      2025.02.04 It is a part of patterns::meshnet now.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "priority_frame.hpp"
#include <pfs/netty/namespace.hpp>
#include <pfs/assert.hpp>
#include <algorithm>
#include <array>
#include <cstdint>
#include <queue>
#include <utility>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

template <int N>
struct frame_calculator;

// Use `writer_queue` instead of `priority_writer_queue`
template <> struct frame_calculator<0>;
template <> struct frame_calculator<1>;

template <>
struct frame_calculator<2>
{
    std::size_t x[2] = {2, 1};
};

template <>
struct frame_calculator<3>
{
    std::size_t x[3] = {4, 2, 1};
};

template <>
struct frame_calculator<4>
{
    std::size_t x[4] = {8, 4, 2, 1};
};

template <>
struct frame_calculator<5>
{
    std::size_t x[5] = {16, 8, 4, 2, 1};
};

template <>
struct frame_calculator<6>
{
    std::size_t x[6] = {32, 16, 8, 4, 2, 1};
};

template <int N, typename FrameCalculator = frame_calculator<N>>
class priority_writer_queue
{
    struct chunk
    {
        std::vector<char> b;
    };

    struct item
    {
        std::queue<chunk> q;
        int frame_counter;
    };

private:
    std::array<item, N> _qpool;   // queue pool
    int _priority_index {0};       // queue pool index (same as priority value)
    std::uint64_t _total_size {0}; // total data size excluding _frame.size()
    std::vector<char> _frame;      // current sending frame

public:
    priority_writer_queue ()
    {
        reset_phase();
    }

private:
    void reset_phase ()
    {
        FrameCalculator fc;

        for (int i = 0; i < N; i++) {
            PFS__ASSERT(fc.x[i] > 0, "priority_writer_queue: frame limit must be greater than zero");
            _qpool[i].frame_counter = fc.x[i];
        }

        _priority_index = 0;
        _frame.clear();
    }

    void acquire_priority ()
    {
        PFS__ASSERT(!empty(), "");

        auto & x = _qpool[_priority_index];

        // TODO May be optimized later
        if (!x.q.empty()) {
            if (x.frame_counter > 0) {
                // Phase is not complete for the current queue
                return;
            } else {
                // Phase is complete for the current queue, need to find next suitable queue
                FrameCalculator fc;
                x.frame_counter = fc.x[_priority_index];
            }
        } else {
            // No more data in the current queue,
            if (x.frame_counter > 0) {
                // Need to find next suitable queue
                ;
            } else {
                // Phase is complete for the current queue, need to find next suitable queue
                FrameCalculator fc;
                x.frame_counter = fc.x[_priority_index];
            }
        }

        _priority_index++;

        if (_priority_index == N)
            _priority_index = 0;

        // Find nearest suitable queue
        while (_priority_index < N) {
            auto & x = _qpool[_priority_index];

            if (x.frame_counter != 0 && !x.q.empty())
                return;

            _priority_index++;
        }

        // A suitable queue must be found since total size is not zero.
        for (_priority_index = 0; _priority_index < N; _priority_index++) {
            auto & x = _qpool[_priority_index];

            if (!x.q.empty())
                return;
        }

        // Must be unreachable
        PFS__TERMINATE(false, "priority_writer_queue: fix acquire_priority() method");
    }

public:
    void enqueue (int priority, char const * data, std::size_t len)
    {
        if (len == 0)
            return;

        _total_size += len;
        _qpool[priority].q.push(chunk{std::vector<char>{data, data + len}});
    }

    void enqueue (int priority, std::vector<char> && data)
    {
        if (data.empty())
            return;

        _total_size += data.size();
        _qpool[priority].q.push(chunk{std::move(data)});
    }

    bool empty () const
    {
        return _total_size == 0 && _frame.empty();
    }

    bool acquire_frame (std::vector<char> & frame, std::size_t frame_size)
    {
        if (!_frame.empty()) {
            frame.insert(frame.end(), _frame.begin(), _frame.end());
            return true;
        }

        if (empty())
            return false;

        acquire_priority();

        // _priority_index already points to non-empty queue (after constructor or `shift` method calling)
        auto & q = _qpool[_priority_index].q;
        auto & b = q.front().b;

        // Recalculate actual frame size according to frame header and bytes left.
        frame_size = (std::min)(b.size() + priority_frame::header_size(), frame_size);

        priority_frame{_priority_index}.serialize(_frame, b.data(), frame_size);
        b.erase(b.begin(), b.begin() + frame_size - priority_frame::header_size());

        PFS__ASSERT(_total_size >= frame_size - priority_frame::header_size(), "");

        _total_size -= (frame_size - priority_frame::header_size());

        frame.insert(frame.end(), _frame.begin(), _frame.end());

        // Check topmost message is processed
        if (b.empty())
            q.pop();

        return true;
    }

    void shift (std::size_t n)
    {
        PFS__ASSERT(n > 0 && n <= _frame.size(), "");

        _frame.erase(_frame.begin(), _frame.begin() + n);

        auto & x = _qpool[_priority_index];

        // Sending message complete
        if (_frame.empty()) {
            x.frame_counter--;
            PFS__ASSERT(x.frame_counter >= 0, "");
        }

        if (empty())
            reset_phase();
    }

public: // static
    static constexpr int priority_count () noexcept
    {
        return N;
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
