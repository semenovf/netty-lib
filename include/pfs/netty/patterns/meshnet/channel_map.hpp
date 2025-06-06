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
#include <pfs/optional.hpp>
#include <functional>
#include <set>

NETTY__NAMESPACE_BEGIN

namespace patterns {
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
    mutable callback_t<void(socket_id)> on_close_socket = [] (socket_id) {
        PFS__TERMINATE(false, "Assign channel_map::on_close_socket callback");
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

    bool insert_reader (node_id id, socket_id sid)
    {
        return _readers.insert(id, sid);
    }

    bool insert_writer (node_id id, socket_id sid)
    {
        return _writers.insert(id, sid);
    }

    bool channel_complete_for (node_id id) const
    {
        return locate_writer(id) != nullptr && locate_reader(id) != nullptr;
    }

    /**
     * Returns @c non-nullopt node ID if the channel is found and was closed.
     */
    pfs::optional<node_id> close_channel (node_id id)
    {
        socket_id const * reader_sid_ptr = locate_reader(id);
        socket_id const * writer_sid_ptr = locate_writer(id);

        bool success = false;

        if (reader_sid_ptr != nullptr) {
            this->on_close_socket(*reader_sid_ptr);
            _readers.erase_by_first(id);
            success = true;
        }

        if (writer_sid_ptr != nullptr) {
            if (!(reader_sid_ptr != nullptr && *writer_sid_ptr == *reader_sid_ptr)) {
                this->on_close_socket(*writer_sid_ptr);
            }

            _writers.erase_by_first(id);
            success = true;
        }

        if (!success)
            return pfs::nullopt;

        return id;
    }

    /**
     * Returns @c non-nullopt node ID if the channel is found and was closed.
     */
    pfs::optional<node_id> close_channel (socket_id sid)
    {
        node_id const * id_ptr = locate_reader(sid);

        if (id_ptr == nullptr)
            id_ptr = locate_writer(sid);

        // Neither the writer nor the reader were found.
        if (id_ptr == nullptr)
            return pfs::nullopt;

        return close_channel(*id_ptr);
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

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END


