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
#include <list>
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
    struct elem
    {
        std::vector<char> b;
        std::size_t cursor;
    };

    struct queue
    {
        std::queue<elem, std::list<elem>> q;
        int frame_limit;
        int frame_counter;
    };

private:
    std::array<queue, N> _qp;      // queue pool
    int _priority_cursor {0};      // queue pool cursor
    std::uint64_t _total_size {0}; // total data size

public:
    priority_writer_queue ()
    {
        FrameCalculator fc;

        for (int i = 0; i < N; i++) {
            PFS__TERMINATE(fc.x[i] > 0, "priority_writer_queue: frame limit must be greater than zero");
            _qp[i].frame_limit = fc.x[i];
            _qp[i].frame_counter = fc.x[i];
        }
    }

private:
    void reset_phase ()
    {
        for (int i = 0; i < N; i++)
            _qp[i].frame_counter = _qp[i].frame_limit;
    }

public:
    void enqueue (int priority, char const * data, std::size_t len)
    {
        if (len == 0)
            return;

        _total_size += len;
        _qp[priority].q.push(elem{std::vector<char>{data, data + len}, 0});
    }

    void enqueue (int priority, std::vector<char> && data)
    {
        if (data.empty())
            return;

        _total_size += data.size();
        _qp[priority].q.push(elem{std::move(data), 0});
    }

    bool empty () const
    {
        return _total_size == 0;
    }

    std::vector<char> frame (std::size_t frame_size) const
    {
        if (empty())
            return std::vector<char>{};

        // _priority_cursor already points to non-empty queue
        auto & q = _qp[_priority_cursor].q;
        auto & front = q.front();
        frame_size = (std::min)(front.b.size() - front.cursor + priority_frame::header_size(), frame_size);

        std::vector<char> chunk;
        auto first = front.b.data() + front.cursor;
        priority_frame{_priority_cursor}.serialize(chunk, first, frame_size);

        return chunk;
    }

    void shift (std::size_t frame_size)
    {
        // Actual length to shift
        auto n = frame_size - priority_frame::header_size();

        _total_size -= n;
        auto & x = _qp[_priority_cursor];
        auto & front = x.q.front();
        front.cursor += n;

        if (front.cursor >= front.b.size())
            x.q.pop();

        x.frame_counter--;

        // Phase is not complete for the current queue
        if (x.frame_counter > 0 && !x.q.empty())
            return;

        // No more data, reset phase
        if (_total_size == 0) {
            _priority_cursor = 0;
            reset_phase();
            return;
        }

        // Phase is complete for the current queue
        x.frame_counter = 0;
        _priority_cursor++;

        // Find nearest suitable queue
        while (_priority_cursor != N) {
            x = _qp[_priority_cursor];

            if (x.frame_counter != 0)
                return;

            _priority_cursor++;
        }

        // _priority_cursor == N => Phase is complete totally
        // Reset phase and find nearest suitable queue
        _priority_cursor = 0;
        reset_phase();

        // A suitable queue must be found since its total size is not zero.
        for (_priority_cursor = 0; _priority_cursor < N; _priority_cursor++) {
            x = _qp[_priority_cursor];

            if (!x.q.empty())
                return;
        }

        // Must be unreachable
        PFS__TERMINATE(false, "priority_writer_queue: fix shift() method");
    }

public: // static
    static constexpr int priority_count () noexcept
    {
        return N;
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
