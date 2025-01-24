////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.01.16 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "namespace.hpp"
#include <pfs/assert.hpp>
#include <cstdint>
#include <map>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

NETTY__NAMESPACE_BEGIN

template <typename Socket, template <typename...> class Map = std::unordered_map>
class socket_pool
{
public:
    using socket_type = Socket;
    using socket_id = typename Socket::socket_id;

private:
    enum class kind_enum { accepted, connected };

    struct account
    {
        socket_type sock;
        kind_enum kind;
    };

    using index_type = typename std::vector<account>::size_type;
    using index_map_type = Map<socket_id, index_type>;

private:
    std::set<index_type> _free_indices;
    std::vector<account> _accounts;
    index_map_type _mapping;
    std::vector<socket_id> _removable;

private:
    void add (socket_type && sock, kind_enum kind)
    {
        if (_free_indices.empty()) {
            _mapping[sock.id()] = _accounts.size();
            _accounts.emplace_back(account{std::move(sock), kind});
        } else {
            auto pos = _free_indices.begin();
            _mapping[sock.id()] = *pos;
            _accounts[*pos] = account{std::move(sock), kind};
            _free_indices.erase(pos);
        }
    }

    account * locate_account (socket_id id)
    {
        auto pos = _mapping.find(id);

        if (pos == _mapping.end())
            return nullptr;

        auto index = pos->second;
        PFS__TERMINATE(index < _accounts.size()
            , "socket_pool::locate_account(): fix implementation of socket_pool");
        PFS__TERMINATE(_free_indices.find(index) == _free_indices.end()
            , "socket_pool::locate_account(): fix implementation of socket_pool");

        auto & acc = _accounts[index];

        PFS__TERMINATE(acc.sock.id() == id
            , "socket_pool::locate_account(): fix implementation of socket_pool");

        return & acc;
    }

public:
    socket_pool () {}

public:
    void add_connected (socket_type && sock)
    {
        add(std::move(sock), kind_enum::connected);
    }

    void add_accepted (socket_type && sock)
    {
        add(std::move(sock), kind_enum::accepted);
    }

    void remove_later (socket_id id)
    {
        _removable.push_back(id);
    }

    void apply_remove ()
    {
        if (!_removable.empty())  {
            for (auto const & id: _removable) {
                auto pos = _mapping.find(id);

                if (pos != _mapping.end()) {
                    auto index = pos->second;
                    _accounts[index].sock = socket_type{};
                    _free_indices.emplace(index);
                    _mapping.erase(pos);
                }
            }

            _removable.clear();
        }
    }

    /**
     * Number of sockets in the pool
     */
    std::size_t count ()
    {
        PFS__TERMINATE(_accounts.size() < (_free_indices.size() - _removable.size())
            , "socket_pool::count(): fix implementation of socket_pool");
        return _accounts.size() - _free_indices.size() - _removable.size();
    }

    socket_type * locate (socket_id id, bool * is_accepted = nullptr)
    {
        auto pacc = locate_account(id);

        if (pacc == nullptr)
            return nullptr;

        if (is_accepted != nullptr)
            *is_accepted = pacc->kind == kind_enum::accepted;

        return & pacc->sock;
    }
};

NETTY__NAMESPACE_END
