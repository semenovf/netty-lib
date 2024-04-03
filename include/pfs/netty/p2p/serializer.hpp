////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2021.10.16 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/endian.hpp"
#include <utility>

#if NETTY_P2P__CEREAL_ENABLED
#   include <cereal/archives/binary.hpp>
#endif

namespace netty {
namespace p2p {

template <typename T>
struct ntoh_wrapper
{
    T * p {nullptr};
    ntoh_wrapper (T & v) : p(& v) {}
};

template <typename T>
inline ntoh_wrapper<T> ntoh (T & v) { return ntoh_wrapper<T>{v}; }

#if NETTY_P2P__CEREAL_ENABLED
template <typename T>
void load (cereal::BinaryInputArchive & ar, ntoh_wrapper<T> & r)
{
    ar >> *r.p;
    *r.p = pfs::to_native_order(*r.p);
}
#endif

}} // namespace netty::p2p

