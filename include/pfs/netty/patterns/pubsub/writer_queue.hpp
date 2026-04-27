
////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.08.07 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "frame.hpp"
#include "../../writer_queue.hpp"

NETTY__NAMESPACE_BEGIN

namespace pubsub {

template <typename SerializerTraits>
using writer_queue = netty::writer_queue<SerializerTraits, frame<SerializerTraits>>;

} // namespace pubsub

NETTY__NAMESPACE_END
