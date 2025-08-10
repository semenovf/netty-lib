////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.08.04 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/netty/socket4_addr.hpp"
#include <zmq.hpp>
#include <cstdint>
#include <vector>

class zmq_subscriber
{
    zmq::context_t _ctx {1};
    zmq::socket_t _sub;

public:
    zmq_subscriber (netty::socket4_addr saddr)
        : _sub(_ctx, ZMQ_SUB)
    {
        std::string addr = "tcp://";
        addr += to_string(saddr);

        _sub.connect(addr.c_str());

        // Subscribe to all messages
        _sub.set(zmq::sockopt::subscribe, "");
    }

public:
    unsigned int step (std::vector<char> & buf);
};
