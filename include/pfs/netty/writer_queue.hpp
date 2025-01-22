////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.01.08 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "namespace.hpp"
#include <algorithm>
#include <list>
#include <queue>
#include <utility>
#include <vector>

NETTY__NAMESPACE_BEGIN

class writer_queue
{
    struct elem
    {
        std::vector<char> b;
        std::size_t cursor;
    };

    using queue_type = std::queue<elem, std::list<elem>>;

private:
    queue_type _q;

public:
    writer_queue () {}

public:
    void enqueue (int /*priority*/, char const * data, std::size_t len)
    {
        if (len == 0)
            return;

        _q.push(elem{std::vector<char>{data, data + len}, 0});
    }

    void enqueue (char const * data, std::size_t len)
    {
        if (len == 0)
            return;

        _q.push(elem{std::vector<char>{data, data + len}, 0});
    }

    void enqueue (int /*priority*/, std::vector<char> && data)
    {
        if (data.empty())
            return;

        _q.push(elem{std::move(data), 0});
    }

    void enqueue (std::vector<char> && data)
    {
        if (data.empty())
            return;

        _q.push(elem{std::move(data), 0});
    }

    bool empty () const
    {
        return _q.empty();
    }

    std::pair<char const *, std::size_t> data_view (std::size_t max_size) const
    {
        if (empty())
            return std::make_pair(nullptr, 0);

        auto & front = _q.front();
        auto size = (std::min)(front.b.size() - front.cursor, max_size);
        return std::make_pair(front.b.data() + front.cursor, size);
    }

    void shift (std::size_t n)
    {
        auto & front = _q.front();
        front.cursor += n;

        if (front.cursor >= front.b.size())
            _q.pop();
    }

public: // static
    static constexpr int priority_count () noexcept
    {
        return 1;
    }
};

NETTY__NAMESPACE_END
