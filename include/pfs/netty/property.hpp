////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.07.29 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <netty/error.hpp>
#include <pfs/i18n.hpp>
#include <pfs/variant.hpp>
#include <cstdint>
#include <map>
#include <string>

namespace netty {

using property_t = pfs::variant<
      bool
    , int
    , float
    , double
    , std::string>; // utf-8 encoded string

using property_map_t = std::map<std::string, property_t>;

template <typename T>
T get_or (property_map_t const & props, std::string const & key, T const & default_value)
{
    auto pos = props.find(key);

    if (pos == props.end())
        return default_value;

    T const * ptr = pfs::get_if<T>(& pos->second);

    if (ptr == nullptr) {
        throw error {
              pfs::make_error_code(pfs::errc::unexpected_data)
            , tr::f_("illegal value for property: {}", key)
        };
    }

    return *ptr;
}

} // namespace netty
