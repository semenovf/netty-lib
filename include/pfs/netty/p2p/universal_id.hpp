////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2021.11.09 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/endian.hpp"
#include "pfs/universal_id.hpp"
#include "pfs/universal_id_crc.hpp"
#include "pfs/universal_id_hash.hpp"

#if NETTY_P2P__CEREAL_ENABLED
#   include <cereal/archives/binary.hpp>
#endif

namespace netty {
namespace p2p {

using universal_id = pfs::universal_id;
    /// @depricated Use `host_id` instead

}} // namespace netty::p2p

#if NETTY_P2P__CEREAL_ENABLED

namespace cereal {

inline void save (cereal::BinaryOutputArchive & ar, netty::p2p::universal_id const & uuid)
{
    auto a = pfs::to_array(uuid, pfs::endian::network);
    ar << cereal::binary_data(a.data(), a.size());
}

inline void load (cereal::BinaryInputArchive & ar, netty::p2p::universal_id & uuid)
{
    decltype(pfs::to_array(netty::p2p::universal_id{}, pfs::endian::network)) a;
    ar >> cereal::binary_data(a.data(), a.size());
    uuid = pfs::make_uuid(a, pfs::endian::network);
}

} // namespace cereal

#endif // NETTY_P2P__CEREAL_ENABLED
