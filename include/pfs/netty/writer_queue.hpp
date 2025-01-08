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
#include <list>
#include <queue>
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
    void enqueue (char const * data, std::size_t len)
    {
        if (len == 0)
            return;

        _q.push(elem{std::vector<char>{data, data + len}, 0});
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

    char const * data () const
    {
        if (empty())
            return nullptr;

        auto & front = _q.front();
        return front.b.data() + front.cursor;
    }

    std::size_t size () const
    {
        if (empty())
            return 0;

        auto & front = _q.front();
        return front.b.size() - front.cursor;
    }

    void shift (std::size_t n)
    {
        if (empty())
            return;

        auto & front = _q.front();
        front.cursor += n;

        if (front.cursor >= front.b.size())
            _q.pop();
    }
};

NETTY__NAMESPACE_END


