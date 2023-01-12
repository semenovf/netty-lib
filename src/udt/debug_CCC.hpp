////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2021.11.07 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "newlib/ccc.hpp"

namespace netty {
namespace udt {

class debug_CCC: public ::CCC
{
public:
    void init () override;
    void close () override;
    void onACK (int32_t) override;
    void onLoss (int32_t const *, std::streamsize) override;
    void onTimeout () override;
    void onPktSent (CPacket const *) override;
    void onPktReceived (CPacket const *) override;
    void processCustomMsg (CPacket const *) override;
};

}} // namespace netty::udt
