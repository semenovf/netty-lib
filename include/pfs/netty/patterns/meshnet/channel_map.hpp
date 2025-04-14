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
#include "unordered_bimap.hpp"
#include <functional>
#include <set>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

template <typename NodeIdTraits, typename Socket>
class channel_map
{
public:
    using node_id_traits = NodeIdTraits;
    using node_id = typename NodeIdTraits::type;
    using node_id_rep = typename NodeIdTraits::rep_type;
    using socket_type = Socket;
    using socket_id = typename socket_type::socket_id;

private:
    unordered_bimap<node_id_rep, socket_id> _readers;
    unordered_bimap<node_id_rep, socket_id> _writers;

    std::function<void(socket_id)> _on_close_socket;

public:
    channel_map () = default;

public:
    template <typename F>
    channel_map & on_close_socket (F && f)
    {
        _on_close_socket = std::forward<F>(f);
        return *this;
    }

    socket_id const * locate_reader (node_id_rep id_rep) const
    {
        return _readers.locate_by_first(id_rep);
    }

    node_id_rep const * locate_reader (socket_id sid) const
    {
        return _readers.locate_by_second(sid);
    }

    socket_id const * locate_writer (node_id_rep id_rep) const
    {
        return _writers.locate_by_first(id_rep);
    }

    node_id_rep const * locate_writer (socket_id sid) const
    {
        return _writers.locate_by_second(sid);
    }

    bool insert_reader (node_id_rep id_rep, socket_id sid)
    {
        return _readers.insert(id_rep, sid);
    }

    bool insert_writer (node_id_rep id_rep, socket_id sid)
    {
        return _writers.insert(id_rep, sid);
    }

    bool channel_complete_for (node_id_rep id_rep) const
    {
        return locate_writer(id_rep) != nullptr && locate_reader(id_rep) != nullptr;
    }

    void close_channel (node_id_rep id_rep)
    {
        socket_id const * reader_sid_ptr = locate_reader(id_rep);
        socket_id const * writer_sid_ptr = locate_writer(id_rep);

        if (reader_sid_ptr != nullptr) {
            _on_close_socket(*reader_sid_ptr);
            _readers.erase_by_first(id_rep);
        }

        if (writer_sid_ptr != nullptr) {
            if (!(reader_sid_ptr != nullptr && *writer_sid_ptr == *reader_sid_ptr)) {
                _on_close_socket(*writer_sid_ptr);
            }

            _writers.erase_by_first(id_rep);
        }
    }

    void close_channel (socket_id sid)
    {
        node_id_rep const * id_rep_ptr = locate_reader(sid);

        if (id_rep_ptr == nullptr)
            id_rep_ptr = locate_writer(sid);

        // Neither the writer nor the reader were found.
        if (id_rep_ptr == nullptr)
            return;

        close_channel(*id_rep_ptr);
    }

    /**
     * Close all channels and clear collection
     */
    void clear ()
    {
        // Collect all node identifiers
        std::set<node_id_rep> tmp;

        if (!_readers.empty())
            _readers.for_each([& tmp] (node_id_rep id_rep, socket_id) {tmp.insert(id_rep);});

        if (!_writers.empty())
            _writers.for_each([& tmp] (node_id_rep id_rep, socket_id) {tmp.insert(id_rep);});

        if (!tmp.empty()) {
            // Close all channels
            for (auto const & id_rep: tmp)
                close_channel(id_rep);

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


