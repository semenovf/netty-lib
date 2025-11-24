////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.03.30 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include "../../callback.hpp"
#include "unordered_bimap.hpp"
#include <pfs/assert.hpp>
#include <functional>
#include <set>
#include <utility>

NETTY__NAMESPACE_BEGIN

namespace meshnet {

template <typename NodeId, typename SocketId>
class channel_map
{
public:
    using node_id = NodeId;
    using socket_id = SocketId;

private:
    unordered_bimap<node_id, socket_id> _readers;
    unordered_bimap<node_id, socket_id> _writers;

public:
    mutable callback_t<void(socket_id)> close_socket = [] (socket_id) {
        PFS__THROW_UNEXPECTED(false, "Assign channel_map::close_socket_cb callback");
    };

public:
    channel_map () = default;

public:
    socket_id const * locate_reader (node_id id) const
    {
        return _readers.locate_by_first(id);
    }

    node_id const * locate_reader (socket_id sid) const
    {
        return _readers.locate_by_second(sid);
    }

    socket_id const * locate_writer (node_id id) const
    {
        return _writers.locate_by_first(id);
    }

    node_id const * locate_writer (socket_id sid) const
    {
        return _writers.locate_by_second(sid);
    }

    bool insert (node_id id, socket_id reader_sid, socket_id writer_sid)
    {
        auto success = _readers.insert(id, reader_sid) && _writers.insert(id, writer_sid);

        if (!success) {
            _readers.erase_by_first(id);
            _writers.erase_by_first(id);
        }

        return success;
    }

    std::pair<bool, node_id> has_channel (socket_id sid) const
    {
        node_id const * id_ptr = locate_reader(sid);

        if (id_ptr == nullptr)
            id_ptr = locate_writer(sid);

        // Neither the writer nor the reader were found.
        if (id_ptr == nullptr)
            return std::make_pair(false, node_id{});

        return std::make_pair(true, *id_ptr);
    }

    /**
     * Returns @c true if channel is found and was closed.
     */
    bool close_channel (node_id id)
    {
        socket_id const * reader_sid_ptr = locate_reader(id);
        socket_id const * writer_sid_ptr = locate_writer(id);

        if (reader_sid_ptr != nullptr && writer_sid_ptr != nullptr) {
            auto reader_sid = *reader_sid_ptr;
            auto writer_sid = *writer_sid_ptr;

            _readers.erase_by_first(id);
            _writers.erase_by_first(id);

            close_socket(reader_sid);
            close_socket(writer_sid);

            return true;
        } else if (reader_sid_ptr == nullptr && writer_sid_ptr == nullptr) {
            ;
        } else {
            PFS__THROW_UNEXPECTED(false, "Fix channel_map");
        }

        return false;
    }

    /**
     * Close all channels and clear collection
     */
    void clear ()
    {
        // Collect all node identifiers
        std::set<node_id> tmp;

        if (!_readers.empty())
            _readers.for_each([& tmp] (node_id id, socket_id) {tmp.insert(id);});

        if (!_writers.empty())
            _writers.for_each([& tmp] (node_id id, socket_id) {tmp.insert(id);});

        if (!tmp.empty()) {
            // Close all channels
            for (auto const & id: tmp)
                close_channel(id);

            _readers.clear();
            _writers.clear();
        }
    }

    template <typename F>
    void for_each_writer (F && f)
    {
        if (!_writers.empty())
            _writers.for_each(std::forward<F>(f));
    }
};

} // namespace meshnet

NETTY__NAMESPACE_END
