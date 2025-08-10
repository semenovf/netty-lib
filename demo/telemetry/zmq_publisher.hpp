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

class zmq_publisher
{
    zmq::context_t _ctx;
    zmq::socket_t _pub;

public:
    zmq_publisher (netty::socket4_addr saddr)
        : _pub(_ctx, ZMQ_PUB)
    {
        std::string addr = "tcp://";
        addr += to_string(saddr);
        _pub.bind(addr.c_str());
    }

public:
    void broadcast (char const * data, std::size_t size)
    {
        zmq::message_t zmsg(size);
        std::memcpy(zmsg.data(), data, size);
        _pub.send(zmsg, zmq::send_flags::none);
    }
};
