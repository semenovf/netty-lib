////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.01.08 Initial version.
//      2025.06.07 It is a part of patterns::meshnet now.
//      2025.09.08 Using chunk type.
//      2025.11.17 `chunk` renamed to `buffer`.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "priority_frame.hpp"
#include "../../namespace.hpp"
#include "../../buffer.hpp"
#include "../../traits/archive_traits.hpp"
#include <pfs/assert.hpp>
#include <algorithm>
#include <queue>
#include <utility>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

template <typename Archive>
class writer_queue
{
    using archive_traits_type = archive_traits<Archive>;
    using priority_frame_type = priority_frame<Archive>;
    using chunk_type = buffer<Archive>;
    using chunk_queue_type = std::queue<chunk_type>;

public:
    using archive_type = typename archive_traits_type::archive_type;

private:
    chunk_queue_type _q;
    archive_type _frame; // Current sending frame

public:
    writer_queue () {}

public:
    void enqueue (int /*priority*/, char const * data, std::size_t len)
    {
        if (len == 0)
            return;

        _q.push(archive_traits_type::make(data, len));
    }

    void enqueue (char const * data, std::size_t len)
    {
        if (len == 0)
            return;

        _q.push(archive_traits_type::make(data, len));
    }

    void enqueue (int /*priority*/, archive_type data)
    {
        if (archive_traits_type::empty(data))
            return;

        _q.push(std::move(data));
    }

    void enqueue (archive_type data)
    {
        if (archive_traits_type::empty(data))
            return;

        _q.push(std::move(data));
    }

    /**
     * Acquires data frame.
     *
     * @param frame_size Requested (initial) frame size.
     * @return bool @c true if frame acquired and stored into @a frame.
     */
    archive_type const & acquire_frame (std::size_t frame_size)
    {
        if (!archive_traits_type::empty(_frame)) {
            PFS__THROW_UNEXPECTED(archive_traits_type::size(_frame) <= frame_size, "");
            return _frame;
        }

        if (_q.empty())
            return _frame; // _frame is empty

        auto & front = _q.front();

        priority_frame_type{}.pack(0, _frame, front, frame_size);

        // Check topmost message is processed
        if (archive_traits_type::empty(front))
            _q.pop();

        return _frame;
    }

    void shift (std::size_t n)
    {
        PFS__THROW_UNEXPECTED(n > 0, "");
        PFS__THROW_UNEXPECTED(n <= archive_traits_type::size(_frame), "");

        if (archive_traits_type::size(_frame) == n) {
            archive_traits_type::clear(_frame);
        } else {
            // _frame.erase(_frame.begin(), _frame.begin() + n);
            archive_traits_type::erase(_frame, 0, n);
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
