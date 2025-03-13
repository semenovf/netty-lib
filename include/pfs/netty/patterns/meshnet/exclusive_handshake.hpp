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
    void handshake_ready (socket_id sid, node_id const & id, bool is_response, bool behind_nat, bool is_gateway)
    {
        // TODO Remote later
        // if (is_response) {
        //     if (is_behind_nat) {
        //         // Responder is behind NAT, so there is no need to choose connection - only one is available.
        //         this->_on_completed(id, sid, handshake_result_enum::reader);
        //         this->_on_completed(id, sid, handshake_result_enum::writer);
        //         return;
        //     } else {
        //         // Responder is not behind NAT, so there is need to choose connection. Select master
        //         // socket by comparison of node identifiers.
        //         if (this->_node.id() < id) {
        //             // Remote client socket is master
        //             this->_on_completed(id, sid, handshake_result_enum::reader);
        //             this->_on_completed(id, sid, handshake_result_enum::writer);
        //         } else {
        //             this->_on_completed(id, sid, handshake_result_enum::unusable);
        //         }
        //     }
        // } else {
        //     if (this->_node.id() > id) { // Server socket is master
        //         this->_on_completed(id, sid, handshake_result_enum::reader);
        //         this->_on_completed(id, sid, handshake_result_enum::writer);
        //     }
        // }

        // Responder is behind NAT, so there is no need to choose connection - only one is available.
        if (behind_nat) {
            this->_on_completed(id, sid, is_gateway, handshake_result_enum::reader);
            this->_on_completed(id, sid, is_gateway, handshake_result_enum::writer);
            return;
        }

        if (is_response) {
            // Responder is not behind NAT, so there is need to choose connection. Select master
            // socket by comparison of node identifiers.
            if (this->_node->id() < id) { // Remote client socket is master
                this->_on_completed(id, sid, is_gateway, handshake_result_enum::reader);
                this->_on_completed(id, sid, is_gateway, handshake_result_enum::writer);
            } else if (this->_node->id() > id) {
                if (this->_node->behind_nat()) {
                    this->_on_completed(id, sid, is_gateway, handshake_result_enum::reader);
                    this->_on_completed(id, sid, is_gateway, handshake_result_enum::writer);
                } else {
                    this->_on_completed(id, sid, is_gateway, handshake_result_enum::unusable);
                }
            } else {
                this->_on_completed(id, sid, is_gateway, handshake_result_enum::duplicated);
            }
        } else {
            if (this->_node->id() > id) { // Server socket is master
                this->_on_completed(id, sid, is_gateway, handshake_result_enum::reader);
                this->_on_completed(id, sid, is_gateway, handshake_result_enum::writer);
            } else if (this->_node->id() == id) {
                this->_on_completed(id, sid, is_gateway, handshake_result_enum::duplicated);
            }
        }
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
