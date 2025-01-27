////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.01.25 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "basic_handshake.hpp"

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

template <typename Node>
class exclusive_handshake: public basic_handshake<exclusive_handshake, Node>
{
    using base_class = basic_handshake<exclusive_handshake, Node>;

    friend base_class;

public:
    using node_id = typename base_class::node_id;
    using socket_id = typename base_class::socket_id;

public:
    exclusive_handshake (Node & node)
        : base_class(node)
    {}

protected:
    void nodeid_ready (socket_id sid, node_id const & id, packet_way_enum way, behind_nat_enum behind_nat)
    {
        if (behind_nat == behind_nat_enum::yes) {
            this->_on_completed(id, sid, handshake_result_enum::reader);
            this->_on_completed(id, sid, handshake_result_enum::writer);
            return;
        }

        if (way == packet_way_enum::response) {
            if (this->_node.id() < id) { // Remote socket is master
                this->_on_completed(id, sid, handshake_result_enum::reader);
                this->_on_completed(id, sid, handshake_result_enum::writer);
            } else {
                this->_on_completed(id, sid, handshake_result_enum::unusable);
            }
        } else {
            if (this->_node.id() > id) { // Accepted socket is master
                this->_on_completed(id, sid, handshake_result_enum::reader);
                this->_on_completed(id, sid, handshake_result_enum::writer);
            }
        }
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
