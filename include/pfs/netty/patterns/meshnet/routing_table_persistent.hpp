////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.03.10 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "routing_table.hpp"
#include <memory>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

template <typename NodeIdTraits, typename SerializerTraits, typename Storage>
class routing_table_persistent: public routing_table<NodeIdTraits, SerializerTraits>
{
    using base_class = routing_table<NodeIdTraits, SerializerTraits>;

public:
    using node_id = typename NodeIdTraits::node_id;
    using serializer_traits = SerializerTraits;

private:
    std::unique_ptr<Storage> _storage;

public:
    routing_table_persistent (node_id id, std::unique_ptr<Storage> && storage)
        : base_class(id)
        , _storage(std::move(storage))
    {
        _storage->load_session([this] () {
            _storage->foreach_gateway([this] (node_id gwid) {
                this->append_gateway(gwid);
            });

            _storage->foreach_route([this] (node_id id, node_id gwid, unsigned int hops) {
                this->add_route(id, gwid, hops);
            });
        });
    }

    ~routing_table_persistent ()
    {
        _storage->save_session([this] () {
            for (auto pos = this->_gateways.begin(); pos != this->_gateways.end(); ++pos)
                _storage->store_gateway(*pos);

            for (auto pos = this->_routes.begin(); pos != this->_routes.end(); ++pos)
                _storage->store_route(pos->first, pos->second.gwid, pos->second.hops);
        });
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
