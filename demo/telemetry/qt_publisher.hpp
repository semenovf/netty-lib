////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.08.05 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/netty/socket4_addr.hpp"
#include <QHostAddress>
#include <QTcpServer>

class qt_publisher
{
    QTcpServer _pub;

public:
    qt_publisher (netty::socket4_addr saddr);

public:
    void broadcast (char const * data, std::size_t size);
};
