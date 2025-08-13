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
#include "../../socket4_addr.hpp"
#include "tag.hpp"
#include "types.hpp"
#include <pfs/log.hpp>
#include <string>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace telemetry {

//
// Serializer requirements: see pfs/netty/patterns/telemetry/serializer.hpp
//

template <typename KeyT, typename Publisher, typename Serializer>
class producer
{
    using publisher_type = Publisher;
    using serializer_type = Serializer;
    using key_type = KeyT;

private:
    publisher_type _pub;
    serializer_type _out;

private: // Callbacks
    callback_t<void (std::string const &)> _on_error
        = [] (std::string const & errstr) { LOGE(TELEMETRY_TAG, "{}", errstr); };

public:
    producer (socket4_addr saddr, int backlog = 100)
        : _pub(saddr, backlog)
    {}

public: // Set callbacks
    /**
     * Sets error callback.
     *
     * @details Callback @a f signature must match:
     *          void (std::string const &)
     */
    template <typename F>
    producer & on_error (F && f)
    {
        _on_error = f;
        _pub.on_error(std::forward<F>(f));
        return *this;
    }

    template <typename F>
    producer & on_accepted (F && f)
    {
        _pub.on_accepted(std::forward<F>(f));
        return *this;
    }

public:
    template <typename T>
    void push (key_type const & key, T const & value)
    {
        _out.pack(key, value);
    }

    void broadcast ()
    {
        _pub.broadcast(_out.data(), _out.size());
        _out.clear();
    }

    template <typename T>
    void broadcast (key_type const & key, T const & value)
    {
        _out.pack(key, value);
        broadcast();
    }

    void interrupt ()
    {
        _pub.interrupt();
    }

    /**
     * @return Number of events occurred.
     */
    unsigned int step ()
    {
        return _pub.step();
    }

    void run (std::chrono::milliseconds loop_interval = std::chrono::milliseconds{10})
    {
        _pub.run(loop_interval);
    }
};

}} // namespace patterns::telemetry

NETTY__NAMESPACE_END
