////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.08.09 Initial version.
//      2025.11.14 Envelope type depends on archive type now.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include "../../envelope.hpp"
#include "../../traits/serializer_traits.hpp"

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace pubsub {

template <typename Archive>
using envelope = netty::envelope<std::uint16_t, serializer_traits<true, Archive>>;

}} // namespace patterns::pubsub

NETTY__NAMESPACE_END
