////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.06.14 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../error.hpp"
#include "../namespace.hpp"
#include <array>

NETTY__NAMESPACE_BEGIN

struct single_priority_distribution {};

template <typename PriorityDistribution>
class priority_tracker
{
public:
    static constexpr std::size_t SIZE = PriorityDistribution::SIZE;

private:
    PriorityDistribution _distribution;
    int _current_priority {0};
    int _current_counter {0};

public:
    priority_tracker () = default;

public:
    int next () noexcept
    {
        if (_current_counter == _distribution[_current_priority]) {
            _current_counter = 0;
            _current_priority++;
        }

        if (_current_priority == PriorityDistribution::SIZE)
            _current_priority = 0;

        _current_counter++;
        return _current_priority;
    }

    /**
     * Jumps to next priority.
     */
    int skip () noexcept
    {
        _current_counter = 0;
        _current_priority++;

        if (_current_priority >= PriorityDistribution::SIZE)
            _current_priority = 0;

        return _current_priority;
    }

    int current () const noexcept
    {
        return _current_priority;
    }

    /**
     * Reset to an initial state.
     */
    void reset ()
    {
        _current_priority = 0;
        _current_counter = 0;
    }
};

template <>
class priority_tracker<single_priority_distribution>
{
public:
    static constexpr std::size_t SIZE = 1;

public:
    int next () noexcept
    {
        return 0;
    }

    void skip () noexcept
    {}

    int current () const noexcept
    {
        return 0;
    }
};

NETTY__NAMESPACE_END
