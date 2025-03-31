////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.02.20 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include <unordered_map>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

// This is not a universal solution for bimapping, only for meshnet node requirements:
// mapping node ID into socket ID and vice versa.
template <typename T1, typename T2>
class unordered_bimap
{
    std::unordered_map<T1, T2> _m1;
    std::unordered_map<T2, T1> _m2;

public:
    bool insert (T1 const & key1, T2 const & key2)
    {
        auto res1 = _m1.insert({key1, key2});
        auto res2 = _m2.insert({key2, key1});

        if (res1.second && res2.second)
            return true;

        _m1.erase(key1);
        _m2.erase(key2);
        return false;
    }

    /**
     * Locates second key by first key
     */
    T2 const * locate_by_first (T1 const & key1) const
    {
        auto pos = _m1.find(key1);
        return pos == _m1.end() ? nullptr : & pos->second;
    }

    /**
     * Locates first key by second key
     */
    T1 const * locate_by_second (T2 const & key2) const
    {
        auto pos = _m2.find(key2);
        return pos == _m2.end() ? nullptr : & pos->second;
    }

    void erase_by_first (T1 const & key1)
    {
        auto ptr = locate_by_first(key1);

        if (ptr != nullptr) {
            _m1.erase(key1);
            _m2.erase(*ptr);
        }
    }

    void erase_by_second (T2 const & key2)
    {
        auto ptr = locate_by_second(key2);

        if (ptr != nullptr) {
            _m1.erase(*ptr);
            _m2.erase(key2);
        }
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
