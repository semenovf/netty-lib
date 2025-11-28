////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.07.29 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../socket4_addr.hpp"
#include "serializer.hpp"
#include "tag.hpp"
#include "types.hpp"
#include <pfs/log.hpp>
#include <mutex>
#include <string>

NETTY__NAMESPACE_BEGIN

namespace telemetry {

//
// Serializer requirements: see pfs/netty/patterns/telemetry/serializer.hpp
//

template <typename KeyT, typename Publisher, typename RecursiveWriterMutex = std::recursive_mutex>
class producer
{
public:
    using serializer_traits_type = typename Publisher::serializer_traits_type;
    using key_type = KeyT;

private:
    using publisher_type = Publisher;
    using archive_type = typename serializer_traits_type::archive_type;
    using serializer_type = typename serializer_traits_type::serializer_type;
    using key_value_serializer_type = key_value_serializer<KeyT, serializer_type>;
    using writer_mutex_type = RecursiveWriterMutex;

private:
    publisher_type _pub;
    archive_type _ar;

    // Writer mutex to protect sending
    writer_mutex_type _writer_mtx;

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
        std::unique_lock<writer_mutex_type> locker{_writer_mtx};
        push_unsafe(key, value);
    }

    void push (key_type const & key, char const * value)
    {
        std::unique_lock<writer_mutex_type> locker{_writer_mtx};
        push_unsafe(key, value);
    }

    template <typename T>
    void push_unsafe (key_type const & key, T const & value)
    {
        serializer_type out {_ar};
        key_value_serializer_type(out, key, value);
    }

    void push_unsafe (key_type const & key, char const * value)
    {
        serializer_type out {_ar};
        key_value_serializer_type(out, key, std::string(value));
    }

    void broadcast ()
    {
        std::unique_lock<writer_mutex_type> locker{_writer_mtx};
        broadcast_unsafe();
    }

    void broadcast_unsafe ()
    {
        _pub.broadcast(_ar.data(), _ar.size());
        _ar.clear();
    }

    template <typename T>
    void broadcast (key_type const & key, T const & value)
    {
        std::unique_lock<writer_mutex_type> locker{_writer_mtx};
        broadcast_unsafe(key, value);
    }

    template <typename T>
    void broadcast_unsafe (key_type const & key, T const & value)
    {
        serializer_type out {_ar};
        key_value_serializer_type(out, key, value);
        broadcast_unsafe();
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
        std::unique_lock<writer_mutex_type> locker{_writer_mtx};
        return _pub.step_unsafe();
    }

    /**
     * @return Number of events occurred.
     */
    unsigned int step_unsafe ()
    {
        return _pub.step_unsafe();
    }

    void run (std::chrono::milliseconds loop_interval = std::chrono::milliseconds{10})
    {
        _pub.run(loop_interval);
    }
};

} // namespace telemetry

NETTY__NAMESPACE_END
