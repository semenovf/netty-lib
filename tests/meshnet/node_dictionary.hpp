////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.12.08 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "transport.hpp"
#include <cstdint>
#include <map>
#include <stdexcept>
#include <vector>

constexpr bool GATEWAY_FLAG = true;
constexpr bool REGULAR_NODE_FLAG = false;

class node_dictionary
{
public:
    struct entry
    {
        std::string name;
        node_id id;
        bool is_gateway {false};
        std::uint16_t port {0};
    };

private:
    std::map<std::string, entry> _nodes;

private:
    node_dictionary (std::initializer_list<entry> init)
    {
        for (auto && x: init) {
            auto res = _nodes.insert({x.name, std::move(x)});
            auto & ent = res.first->second;
        }
    }

public:
    node_dictionary ()
        : node_dictionary({
            // Gateways
              { "a", "01JQN2NGY47H3R81Y9SG0F0A00"_uuid, GATEWAY_FLAG, 4210 }
            , { "b", "01JQN2NGY47H3R81Y9SG0F0B00"_uuid, GATEWAY_FLAG, 4220 }
            , { "c", "01JQN2NGY47H3R81Y9SG0F0C00"_uuid, GATEWAY_FLAG, 4230 }
            , { "d", "01JQN2NGY47H3R81Y9SG0F0D00"_uuid, GATEWAY_FLAG, 4240 }
            , { "e", "01JQN2NGY47H3R81Y9SG0F0E00"_uuid, GATEWAY_FLAG, 4250 }

            // Regular nodes
            , { "A0", "01JQC29M6RC2EVS1ZST11P0VA0"_uuid, REGULAR_NODE_FLAG, 4211 }
            , { "A1", "01JQC29M6RC2EVS1ZST11P0VA1"_uuid, REGULAR_NODE_FLAG, 4212 }
            , { "B0", "01JQC29M6RC2EVS1ZST11P0VB0"_uuid, REGULAR_NODE_FLAG, 4221 }
            , { "B1", "01JQC29M6RC2EVS1ZST11P0VB1"_uuid, REGULAR_NODE_FLAG, 4222 }
            , { "C0", "01JQC29M6RC2EVS1ZST11P0VC0"_uuid, REGULAR_NODE_FLAG, 4231 }
            , { "C1", "01JQC29M6RC2EVS1ZST11P0VC1"_uuid, REGULAR_NODE_FLAG, 4232 }
            , { "D0", "01JQC29M6RC2EVS1ZST11P0VD0"_uuid, REGULAR_NODE_FLAG, 4241 }
            , { "D1", "01JQC29M6RC2EVS1ZST11P0VD1"_uuid, REGULAR_NODE_FLAG, 4242 }

            // For test duplication
            , { "A0_dup", "01JQC29M6RC2EVS1ZST11P0VA0"_uuid, REGULAR_NODE_FLAG, 4213 }
        })
    {}

public:
    entry const & get_entry (std::string const & name) const
    {
        return _nodes.at(name);
    }

    entry const & get_entry (node_id id) const
    {
        for (auto const & x: _nodes) {
            if (x.second.id == id)
                return x.second;
        }

        throw std::out_of_range {"node node found by id: " + to_string(id)};
    }

public:
    static std::vector<node_id> generate_node_ids (int count)
    {
        std::vector<node_id> ids;

        for (int i = 0; i < count; i++) {
            auto id = pfs::generate_uuid();
            ids.push_back(id);
        }

        return ids;
    }
};
