////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.01.08 Initial version.
//      2025.06.07 It is a part of patterns::meshnet now.
//      2025.09.08 Using chunk type.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include "../../chunk.hpp"
#include "priority_frame.hpp"
#include <pfs/assert.hpp>
#include <algorithm>
#include <queue>
#include <utility>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

class writer_queue
{
    using chunk_type = chunk;
    using chunk_queue_type = std::queue<chunk_type>;

private:
    chunk_queue_type _q;
    std::vector<char> _frame; // Current sending frame

public:
    writer_queue () {}

public:
    void enqueue (int /*priority*/, char const * data, std::size_t len)
    {
        if (len == 0)
            return;

        _q.push(std::vector<char>{data, data + len});
    }

    void enqueue (char const * data, std::size_t len)
    {
        if (len == 0)
            return;

        _q.push(std::vector<char>{data, data + len});
    }

    void enqueue (int /*priority*/, std::vector<char> && data)
    {
        if (data.empty())
            return;

        _q.push(std::move(data));
    }

    void enqueue (std::vector<char> && data)
    {
        if (data.empty())
            return;

        _q.push(std::move(data));
    }

    /**
     * Acquires data frame.
     *
     * @param frame_size Requested (initial) frame size.
     * @return bool @c true if frame acquired and stored into @a frame.
     */
    std::vector<char> const & acquire_frame (std::size_t frame_size)
    {
        if (!_frame.empty()) {
            PFS__THROW_UNEXPECTED(_frame.size() <= frame_size, "");
            return _frame;
        }

        if (_q.empty())
            return _frame; // _frame is empty

        auto & front = _q.front();

        priority_frame{}.pack(0, _frame, front, frame_size);

        // Check topmost message is processed
        if (front.empty())
            _q.pop();

        return _frame;
    }

    void shift (std::size_t n)
    {
        PFS__THROW_UNEXPECTED(n > 0, "");
        PFS__THROW_UNEXPECTED(n <= _frame.size(), "");

        if (_frame.size() == n) {
            _frame.clear();
        } else {
            _frame.erase(_frame.begin(), _frame.begin() + n);
        }
    }

public: // static
    static constexpr int priority_count () noexcept
    {
        return 1;
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
