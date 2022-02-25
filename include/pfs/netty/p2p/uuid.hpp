////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.11.09 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/endian.hpp"
#include "pfs/uuid.hpp"
#include "pfs/uuid_crc.hpp"
#include "pfs/uuid_hash.hpp"
#include <cereal/archives/binary.hpp>
#include <utility>
#include <cassert>

namespace netty {
namespace p2p {

using uuid_t = pfs::uuid_t;

}} // namespace netty::p2p

namespace cereal {

inline void save (cereal::BinaryOutputArchive & ar, netty::p2p::uuid_t const & uuid)
{
    auto a = pfs::to_array(uuid, pfs::endian::network);
    ar << cereal::binary_data(a.data(), a.size());
}

inline void load (cereal::BinaryInputArchive & ar, netty::p2p::uuid_t & uuid)
{
    decltype(pfs::to_array(netty::p2p::uuid_t{}, pfs::endian::network)) a;
    ar >> cereal::binary_data(a.data(), a.size());
    uuid = pfs::make_uuid(a, pfs::endian::network);
}

} // namespace cereal
