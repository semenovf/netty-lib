////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.07.29 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/netty/telemetry/visitor.hpp"
#include <pfs/assert.hpp>
#include <pfs/log.hpp>
#include <atomic>
#include <condition_variable>
#include <string>

extern std::atomic_int g_complete_counter;
extern std::condition_variable g_cv;

class visitor: public netty::telemetry::visitor<std::string>
{
public:
    void on (key_type const & key, netty::telemetry::int8_t value) override
    {
        LOGD("***", "{}: {}", key, value);
        PFS__ASSERT(key == "one", "Unexpected key");
        PFS__ASSERT(value == (std::numeric_limits<std::int8_t>::max)(), "Unexpected value");
    }

    void on (key_type const & key, netty::telemetry::int16_t value) override
    {
        LOGD("***", "{}: {}", key, value);
        PFS__ASSERT(key == "two", "Unexpected key");
        PFS__ASSERT(value == (std::numeric_limits<std::int16_t>::max)(), "Unexpected value");
    }

    void on (key_type const & key, netty::telemetry::int32_t value) override
    {
        LOGD("***", "{}: {}", key, value);
        PFS__ASSERT(key == "three", "Unexpected key");
        PFS__ASSERT(value == (std::numeric_limits<std::int32_t>::max)(), "Unexpected value");
    }

    void on (key_type const & key, netty::telemetry::int64_t value) override
    {
        LOGD("***", "{}: {}", key, value);
        PFS__ASSERT(key == "four", "Unexpected key");
        PFS__ASSERT(value == (std::numeric_limits<std::int64_t>::max)(), "Unexpected value");
    }

    void on (key_type const & key, netty::telemetry::float32_t value) override
    {
        LOGD("***", "{}: {}", key, value);
        PFS__ASSERT(key == "five", "Unexpected key");
        PFS__ASSERT(value == netty::telemetry::float32_t{3.14159}, "Unexpected value");
    }

    void on (key_type const & key, netty::telemetry::float64_t value) override
    {
        LOGD("***", "{}: {}", key, value);
        PFS__ASSERT(key == "six", "Unexpected key");
        PFS__ASSERT(value == netty::telemetry::float64_t{2.71828}, "Unexpected value");
    }

    void on (key_type const & key, netty::telemetry::string_t const & value) override
    {
        if (key == "") {
            g_complete_counter.fetch_add(1);
            g_cv.notify_one();
            return;
        }

        LOGD("***", "{}: {}", key, value);
        PFS__ASSERT(key == "seven", "Unexpected key");
        PFS__ASSERT(value == "Hello", "Unexpected value");
    }

    void on_error (std::string const & errstr)
    {
        LOGE("", "{}", errstr);
    }
};
