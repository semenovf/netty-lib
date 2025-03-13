////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.03.13 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include "alive_info.hpp"
#include "protocol.hpp"
#include <cstdint>
#include <utility>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

template <typename NodeIdTraits, typename SerializerTraits>
class alive_processor
{
public:
    using node_id = typename NodeIdTraits::node_id;
    using serializer_traits = SerializerTraits;

private:
    std::pair<std::uint64_t, std::uint64_t> _id_pair; // Node ID representation

public:
    alive_processor (node_id id)
    {
        _id_pair.first = NodeIdTraits::high(id);
        _id_pair.second = NodeIdTraits::low(id);
    }

    alive_processor (alive_processor const &) = delete;
    alive_processor (alive_processor &&) = delete;
    alive_processor & operator = (alive_processor const &) = delete;
    alive_processor & operator = (alive_processor &&) = delete;

    ~alive_processor () = default;

public:
    std::vector<char> serialize ()
    {
        auto out = serializer_traits::make_serializer();
        alive_packet pkt;
        pkt.ainfo.id = _id_pair;
        pkt.serialize(out);
        return out.take();
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
