////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.10.08 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/uuid.hpp"
#include <unordered_map>
#include <mutex>

namespace pfs {
namespace net {
namespace p2p {

template <typename SharedEndpoint>
class endpoints_table
{
    using shared_endpoint_type = SharedEndpoint;

private:
    std::unordered_map<uuid_t, shared_endpoint_type> _table;

public:
    endpoints_table () = default;

    void insert_or_replace (shared_endpoint_type ep)
    {
        auto it = _table.find(ep->peer_uuid());

        if (it != _table.end()) {
            it->second->disconnect();
            _table.erase(it);
        }

        _table.insert({ep->peer_uuid(), ep});
    }

//     std::shared_ptr<Endpoint> fetch_and_erase (uuid_t uuid)
//     {
//         auto it = _map.find(uuid);
//         std::shared_ptr<Endpoint> ep;
//
//         if (it != _map.end()) {
//             ep = it->second;
//             _map.erase(it);
//         }
//
//         return ep;
//     }

//     std::shared_ptr<Endpoint> operator [] (uuid_t uuid)
//     {
//         auto it = _map.find(uuid);
//         return (it != _map.end())
//             ? it->second
//             : std::shared_ptr<Endpoint>{};
//     }
};

}}} // namespace pfs::net::p2p
