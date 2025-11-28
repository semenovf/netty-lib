////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.07.29 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include <cstdint>
#include <string>

NETTY__NAMESPACE_BEGIN

namespace telemetry {

using int8_t = std::int8_t;
using int16_t = std::int16_t;
using int32_t = std::int32_t;
using int64_t = std::int64_t;
using float32_t = float;
using float64_t = double;
using string_t = std::string;

static_assert(sizeof(float32_t) == 4, "Expected size of float32_t 4 bytes");
static_assert(sizeof(float64_t) == 8, "Expected size of float64_t 8 bytes");

template <typename T>
std::int8_t type_of () noexcept;

template <> constexpr std::int8_t type_of<bool> () noexcept      { return 1; }
template <> constexpr std::int8_t type_of<int8_t> () noexcept    { return 2; }
template <> constexpr std::int8_t type_of<int16_t> () noexcept   { return 3; }
template <> constexpr std::int8_t type_of<int32_t> () noexcept   { return 4; }
template <> constexpr std::int8_t type_of<int64_t> () noexcept   { return 5; }
template <> constexpr std::int8_t type_of<float32_t> () noexcept { return 6; }
template <> constexpr std::int8_t type_of<float64_t> () noexcept { return 7; }
template <> constexpr std::int8_t type_of<string_t> () noexcept  { return 8; }

} // namespace telemetry

NETTY__NAMESPACE_END
