////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.08.09 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include "../../envelope.hpp"

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace pubsub {

using envelope_t = envelope16be_t;

}} // namespace patterns::pubsub

NETTY__NAMESPACE_END
