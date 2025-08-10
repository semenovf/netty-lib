////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.08.05 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include "types.hpp"
#include <string>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace telemetry {

class visitor
{
public:
    using key_type = std::string;

public:
    virtual void on (key_type const & key, bool value) = 0;
    virtual void on (key_type const & key, int8_t value) = 0;
    virtual void on (key_type const & key, int16_t value) = 0;
    virtual void on (key_type const & key, int32_t value) = 0;
    virtual void on (key_type const & key, int64_t value) = 0;
    virtual void on (key_type const & key, float32_t value) = 0;
    virtual void on (key_type const & key, float64_t value) = 0;
    virtual void on (key_type const & key, string_t const & value) = 0;

    virtual void on_error (std::string const & errstr) = 0;
};

}} // namespace patterns::telemetry

NETTY__NAMESPACE_END
