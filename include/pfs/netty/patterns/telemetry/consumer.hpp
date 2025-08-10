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
#include "visitor.hpp"
#include "tag.hpp"
#include <chrono>
#include <memory>
#include <string>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace telemetry {

template <typename Subscriber, typename Deserializer>
class consumer
{
    using subscriber_type = Subscriber;
    using deserializer_type = Deserializer;
    using key_type = std::string;

private:
    subscriber_type _sub;
    std::shared_ptr<visitor> _visitor;

private: // Callbacks
    callback_t<void (std::string const &)> _on_error
        = [] (std::string const & errstr) { LOGE(TELEMETRY_TAG, "{}", errstr); };

public:
    consumer ()
    {
        _sub.on_data_ready([this] (std::vector<char> && data) {
            deserializer_type{data.data(), data.size(), _visitor};
        });
    }

    consumer (std::shared_ptr<visitor> v)
        : consumer()
    {
        _visitor = v;
    }

public: // Set callbacks
    /**
     * Sets error callback.
     *
     * @details Callback @a f signature must match:
     *          void (std::string const &)
     */
    template <typename F>
    consumer & on_error (F && f)
    {
        _on_error = f;
        _sub.on_error(std::forward<F>(f));
        return *this;
    }

public:
    void set_visitor (std::shared_ptr<visitor> v)
    {
        _visitor = v;
    }

    /**
     * Connects to producer.
     */
    bool connect (socket4_addr remote_saddr)
    {
        return _sub.connect(remote_saddr);
    }

    /**
     * Connects to publisher.
     */
    bool connect (netty::socket4_addr remote_saddr, netty::inet4_addr local_addr)
    {
        return _sub.connect(remote_saddr, local_addr);
    }

    void interrupt ()
    {
        _sub.interrupt();
    }

    /**
     * @return Number of events occurred.
     */
    unsigned int step ()
    {
        return _sub.step();
    }

    void run (std::chrono::milliseconds loop_interval = std::chrono::milliseconds{10})
    {
        _sub.run(loop_interval);
    }
};

}} // namespace patterns::telemetry

NETTY__NAMESPACE_END
