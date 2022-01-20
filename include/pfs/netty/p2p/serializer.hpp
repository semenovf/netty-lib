////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.10.16 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/endian.hpp"
#include <cereal/archives/binary.hpp>
#include <utility>

namespace netty {
namespace p2p {

template <typename T>
struct ntoh_wrapper
{
    T * p {nullptr};
    ntoh_wrapper (T & v) : p(& v) {}
};

template <typename T>
void load (cereal::BinaryInputArchive & ar, ntoh_wrapper<T> & r)
{
    ar >> *r.p;
    *r.p = pfs::to_native_order(*r.p);
}

}} // namespace netty::p2p

