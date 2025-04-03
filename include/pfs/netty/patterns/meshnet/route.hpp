////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.03.25 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include "node_id_rep.hpp"
#include "route_info.hpp"
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

enum class route_order_enum { direct, reverse };

class route_segment
{
private:
    node_id_rep _a;
    node_id_rep _b;

public:
    route_segment (node_id_rep a, node_id_rep b): _a(a), _b(b) {}

public:
    bool contains (node_id_rep id) const noexcept
    {
        return _a == id || _b == id;
    }

    bool operator == (route_segment const & other) const noexcept
    {
        return (_a == other._a && _b == other._b)
            || (_a == other._b && _b == other._a);
    }

    bool operator != (route_segment const & other) const noexcept
    {
        return !this->operator == (other);
    }
};

class route
{
    struct segment_item
    {
        route_segment rseg;
        bool connected;
    };

private:
    // Store routes as vectors of segments
    std::vector<segment_item> _route;

    // Good route flag - all segments are in connected state.
    bool _good {true};

public:
    // route (route_info const & rinfo, route_order_enum order)
    // {
    //     auto count = rinfo.route.size();
    //
    //     if (count == 1) { // Only one gateway: segment is `a-a`
    //         segment_item item {route_segment {rinfo.route[0], rinfo.route[0]}, true};
    //         _route.push_back(std::move(item));
    //     } else if (count > 1) { // count - 1 segments: segment is `a-b`
    //         if (order == route_order_enum::direct) {
    //             for (int i = 1; i < count; i++) {
    //                 segment_item item {route_segment {rinfo.route[i - 1], rinfo.route[i]}, true};
    //                 _route.push_back(std::move(item));
    //             }
    //         } else {
    //             for (int i = count - 1; i > 0; i--) {
    //                 segment_item item {route_segment {rinfo.route[i], rinfo.route[i - 1]}, true};
    //                 _route.push_back(std::move(item));
    //             }
    //         }
    //     }
    // }

    template <typename It>
    route (It first, It last)
    {
        if (first == last)
            return;

        auto pos = first;
        ++pos;

        if (pos == last) {
            segment_item item {route_segment {*first, *first}, true};
            _route.push_back(std::move(item));
        } else {
            while (pos != last) {
                segment_item item {route_segment {*pos, *first}, true};
                _route.push_back(std::move(item));
                ++pos;
                ++first;
            }
        }
    }

    route () = default;
    route (route const &) = delete;
    route (route &&) = default;
    route & operator = (route const &) = delete;
    route & operator = (route &&) = default;

    ~route () = default;

private:
    int find_segment (route_segment const & rseg) const noexcept
    {
        for (int i = 0; i < _route.size(); i++) {
            if (_route[i].rseg == rseg)
                return i;
        }

        return -1;
    }

public:
    bool good () const noexcept
    {
        return _good;
    }

    bool equals_to (route const & other) const noexcept
    {
        if (_route.empty() && other._route.empty())
            return true;

        if (_route.size() != other._route.size())
            return false;

        for (int i = 0; i < _route.size(); i++) {
            if (_route[i].rseg != other._route[i].rseg)
                return false;
        }

        return true;
    }

    /**
     * Set segment connected state to @a value and update `good` flag.
     */
    void set_connected (route_segment const & rseg, bool value) noexcept
    {
        auto i = find_segment(rseg);

        if (i >= 0) {
            _route[i].connected = value;

            if (value) {
                auto good = true;

                for (int i = 0; i < _route.size(); i++) {
                    if (!_route[i].connected) {
                        good = false;
                        break;
                    }
                }

                _good = good;
            } else {
                _good = false;
            }
        }
    }

    bool connected (route_segment const & rseg) const noexcept
    {
        auto i = find_segment(rseg);

        if (i >= 0)
            return _route[i].connected;

        return false;
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
