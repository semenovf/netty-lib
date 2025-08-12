
////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.08.07 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include "../../envelope.hpp"
#include <pfs/assert.hpp>
#include <algorithm>
#include <queue>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace pubsub {

// NOTE This is practically the same class as meshnet::writer_queue,
// excluding using `envelope` instead of `priority_frame`.
// TODO It is necessary to generalize this implementation of `writer_queue`
//      (see `meshnet::priority_writer_queue` also)

class writer_queue
{
    using chunk_type = std::vector<char>;
    using chunk_queue_type = std::queue<chunk_type>;
    using envelope_type = envelope16be_t;

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

    void enqueue (int /*priority*/, std::vector<char> data)
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

        // Calculate actual frame size
        // frame_size <= envelope::MIN_SIZE + payload_size
        frame_size = (std::min)(front.size() + envelope_type::MIN_SIZE, frame_size);
        std::uint16_t payload_size = frame_size - envelope_type::MIN_SIZE;

        PFS__THROW_UNEXPECTED(frame_size > envelope_type::MIN_SIZE
            , "Fix writer_queue::acquire_frame algorithm");

        _frame.clear();
        envelope_type ep;
        ep.pack(_frame, front.data(), payload_size);
        front.erase(front.begin(), front.begin() + payload_size);

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

}} // namespace patterns::pubsub

NETTY__NAMESPACE_END
