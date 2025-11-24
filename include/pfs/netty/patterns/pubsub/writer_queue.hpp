
////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.08.07 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "envelope.hpp"
#include "../../namespace.hpp"
#include "../../traits/archive_traits.hpp"
#include <pfs/assert.hpp>
#include <algorithm>
#include <queue>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace pubsub {

// NOTE This is practically the same class as meshnet::writer_queue, excluding using `envelope`
//      instead of `priority_frame`.
// TODO It is necessary to generalize this implementation of `writer_queue`
//      (see `meshnet::priority_writer_queue` also)

template <typename Archive>
class writer_queue
{
    using archive_traits_type = archive_traits<Archive>;
    using chunk_type = typename archive_traits_type::archive_type;;
    using chunk_queue_type = std::queue<chunk_type>;
    using envelope_type = envelope<Archive>;

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

    void enqueue (int /*priority*/, archive_type data)
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
    archive_type acquire_frame (std::size_t frame_size)
    {
        if (!archive_traits_type::empty(_frame)) {
            PFS__THROW_UNEXPECTED(archive_traits_type::size(_frame) <= frame_size, "");
            return _frame;
        }

        if (_q.empty())
            return _frame; // _frame is empty

        auto & front = _q.front();

        // Calculate actual frame size
        // frame_size <= envelope::min_size() + payload_size
        frame_size = (std::min)(archive_traits_type::size(front) + envelope_type::min_size(), frame_size);
        std::uint16_t payload_size = frame_size - envelope_type::min_size();

        PFS__THROW_UNEXPECTED(frame_size > envelope_type::min_size()
            , "Fix writer_queue::acquire_frame algorithm");

        archive_traits_type::clear(_frame);
        envelope_type ep;
        ep.pack(_frame, archive_traits_type::data(front), payload_size);

        //front.erase(front.begin(), front.begin() + payload_size);
        archive_traits_type::erase(front, 0, payload_size);

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
            //_frame.erase(_frame.begin(), _frame.begin() + n);
            archive_traits_type::erase(_frame, 0, n);
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
