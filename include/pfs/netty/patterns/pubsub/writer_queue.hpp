
////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.08.07 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "frame.hpp"
#include <pfs/assert.hpp>
#include <algorithm>
#include <queue>

NETTY__NAMESPACE_BEGIN

namespace pubsub {

// TODO It is necessary to generalize this implementation of `writer_queue`
//      (see `meshnet::priority_writer_queue` also)

template <typename SerializerTraits>
class writer_queue
{
public:
    using serializer_traits_type = SerializerTraits;
    using archive_type = typename serializer_traits_type::archive_type;

private:
    using chunk_queue_type = std::queue<archive_type>;
    using frame_type = frame<serializer_traits_type>;

private:
    chunk_queue_type _q;
    archive_type _frame; // Current sending frame

public:
    writer_queue () {}

public:
    // Writer Pool requirement
    //                      |
    //                      v
    void enqueue (int /*priority*/, char const * data, std::size_t size)
    {
        if (size == 0)
            return;

        _q.push(archive_type{data, size});
    }

    // Writer Pool requirement
    //                      |
    //                      v
    void enqueue (int /*priority*/, archive_type data)
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
    archive_type acquire_frame (std::size_t frame_size)
    {
        if (!_frame.empty()) {
            PFS__THROW_UNEXPECTED(_frame.size() <= frame_size, "");
            return _frame;
        }

        if (_q.empty())
            return _frame; // _frame is empty here

        auto & front = _q.front();

        PFS__THROW_UNEXPECTED(_frame.empty(), "");

        frame_type::pack(_frame, front, frame_size);

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
            _frame.erase_front(n);
        }
    }

public: // static
    // Writer Pool requirement --+
    //                           |
    //                           v
    static constexpr int priority_count () noexcept
    {
        return 1;
    }

};

} // namespace pubsub

NETTY__NAMESPACE_END
