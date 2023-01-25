////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2021.11.07 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "debug_CCC.hpp"
#include "pfs/log.hpp"

namespace netty {
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
    LOG_TRACE_3("debug_CCC::init()", 0);
}

// Functionality:
//    Callback function to be called when a UDT connection is closed.
// Parameters:
//    None.
// Returned value:
//    None.
void debug_CCC::close ()
{
    LOG_TRACE_3("debug_CCC::close()", 0);
}

// Functionality:
//    Callback function to be called when an ACK packet is received.
// Parameters:
//    0) [in] ackno: the data sequence number acknowledged by this ACK.
// Returned value:
//    None.
void debug_CCC::onACK (int32_t ackno)
{
    LOG_TRACE_3("debug_CCC::onACK(ackno={})", ackno);
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
    LOG_TRACE_3("debug_CCC::onLoss()", 0);
}

// Functionality:
//    Callback function to be called when a timeout event occurs.
// Parameters:
//    None.
// Returned value:
//    None.
void debug_CCC::onTimeout ()
{
    LOG_TRACE_3("debug_CCC::onTimeout()", 0);
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
    LOG_TRACE_3("debug_CCC::onPktSent()", 0);
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
    LOG_TRACE_3("debug_CCC::onPktReceived()", 0);
}

// Functionality:
//    Callback function to Process a user defined packet.
// Parameters:
//    0) [in] pkt: the user defined packet.
// Returned value:
//    None.
void debug_CCC::processCustomMsg (CPacket const *)
{
    LOG_TRACE_3("debug_CCC::processCustomMsg()", 0);
}

}} // namespace netty::p2p::udt
