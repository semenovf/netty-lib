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
#include <pfs/assert.hpp>
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
    node_id_rep first () const noexcept
    {
        return _a;
    }

    node_id_rep second () const noexcept
    {
        return _b;
    }

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

    /**
     * Convert route with segments to route as the chain of nodes (as represented by route_info::route)
     */
    std::vector<node_id_rep> convert () const
    {
        if (_route.empty())
            return std::vector<node_id_rep>{};

        std::vector<node_id_rep> result;

        if (_route.size() == 1) {
            PFS__TERMINATE(_route[0].rseg.first() == _route[0].rseg.second()
                , "Fix meshnet::route algorithm");
            result.push_back(_route[0].rseg.first());
        } else {
            result.push_back(_route.front().rseg.first());

            for (int i = 1; i < _route.size() - 1; i++) {
                PFS__TERMINATE(_route[i - 1].rseg.second() == _route[i].rseg.first()
                    , "Fix meshnet::route algorithm");

                result.push_back(_route[i].rseg.first());
            }

            result.push_back(_route.back().rseg.second());
        }

        return result;
    }

    /**
     * Returns first node in the first segment. It is a gateway.
     */
    node_id_rep gateway () const
    {
        PFS__TERMINATE(!_route.empty(), "Fix meshnet::route algorithm");
        return _route[0].rseg.first();
    }

    std::size_t size () const noexcept
    {
        return _route.size();
    }

    /**
     * Synonym for size().
     */
    std::size_t hops () const noexcept
    {
        return _route.size();
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
