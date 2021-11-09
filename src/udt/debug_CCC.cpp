////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.11.07 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "debug_CCC.hpp"

#define PFS_NET__P2P_TRACE_LEVEL 3
#include "pfs/net/p2p/trace.hpp"

namespace pfs {
namespace net {
namespace p2p {
namespace udt {

// Functionality:
//    Callback function to be called (only) at the start of a UDT connection.
//    note that this is different from CCC(), which is always called.
// Parameters:
//    None.
// Returned value:
//    None.
void debug_CCC::init ()
{
    TRACE_D("--- debug_CCC::init() ---", 0);
}

// Functionality:
//    Callback function to be called when a UDT connection is closed.
// Parameters:
//    None.
// Returned value:
//    None.
void debug_CCC::close ()
{
    TRACE_D("--- debug_CCC::close() ---", 0);
}

// Functionality:
//    Callback function to be called when an ACK packet is received.
// Parameters:
//    0) [in] ackno: the data sequence number acknowledged by this ACK.
// Returned value:
//    None.
void debug_CCC::onACK (int32_t ackno)
{
    TRACE_D("--- debug_CCC::onACK(ackno={}) ---", ackno);
}

// Functionality:
//    Callback function to be called when a loss report is received.
// Parameters:
//    0) [in] losslist: list of sequence number of packets, in the format describled in packet.cpp.
//    1) [in] size: length of the loss list.
// Returned value:
//    None.
void debug_CCC::onLoss (int32_t const *, int)
{
    TRACE_D("--- debug_CCC::onLoss() ---", 0);
}

// Functionality:
//    Callback function to be called when a timeout event occurs.
// Parameters:
//    None.
// Returned value:
//    None.
void debug_CCC::onTimeout ()
{
    TRACE_D("--- debug_CCC::onTimeout() ---", 0);
}

// Functionality:
//    Callback function to be called when a data is sent.
// Parameters:
//    0) [in] seqno: the data sequence number.
//    1) [in] size: the payload size.
// Returned value:
//    None.
void debug_CCC::onPktSent (CPacket const *)
{
    TRACE_D("--- debug_CCC::onPktSent() ---", 0);
}

// Functionality:
//    Callback function to be called when a data is received.
// Parameters:
//    0) [in] seqno: the data sequence number.
//    1) [in] size: the payload size.
// Returned value:
//    None.
void debug_CCC::onPktReceived (CPacket const *)
{
    TRACE_D("--- debug_CCC::onPktReceived() ---", 0);
}

// Functionality:
//    Callback function to Process a user defined packet.
// Parameters:
//    0) [in] pkt: the user defined packet.
// Returned value:
//    None.
void debug_CCC::processCustomMsg (CPacket const *)
{
    TRACE_D("--- debug_CCC::processCustomMsg() ---", 0);
}

}}}} // namespace pfs::net::p2p::udt
