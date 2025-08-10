
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
#include <QByteArray>
#include <QEventLoop>
#include <QTcpSocket>
#include <cstdint>
#include <vector>

class qt_subscriber
{
    QByteArray _buf; // input buffer
    QEventLoop _loop;
    QTcpSocket _sub;
    unsigned int _step_result {0};

public:
    qt_subscriber (netty::socket4_addr saddr);

public:
    unsigned int step (std::vector<char> & buf);
};
