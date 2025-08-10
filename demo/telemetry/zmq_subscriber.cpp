////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.08.04 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "zmq_subscriber.hpp"
#include <cstring>

unsigned int zmq_subscriber::step (std::vector<char> & buf)
{
    zmq::message_t zmsg;
    zmq::recv_result_t rc = _sub.recv(zmsg, zmq::recv_flags::dontwait);

    if (!rc.has_value())
        return 0;

    auto off = buf.size();
    buf.resize(off + zmsg.size());
    std::memcpy(buf.data() + off, static_cast<char const *>(zmsg.data()), zmsg.size());

    return 1;
}
