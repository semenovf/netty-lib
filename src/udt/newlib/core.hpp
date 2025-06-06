/*****************************************************************************
Copyright (c) 2001 - 2011, The Board of Trustees of the University of Illinois.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above
  copyright notice, this list of conditions and the
  following disclaimer.

* Redistributions in binary form must reproduce the
  above copyright notice, this list of conditions
  and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of the University of Illinois
  nor the names of its contributors may be used to
  endorse or promote products derived from this
  software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

/*****************************************************************************
written by
   Yunhong Gu, last updated 02/28/2012
*****************************************************************************/
#pragma once
#include "udt.hpp"
#include "common.hpp"
#include "list.hpp"
#include "buffer.hpp"
#include "window.hpp"
#include "packet.hpp"
#include "channel.hpp"
#include "api.hpp"
#include "ccc.hpp"
#include "cache.hpp"
#include "queue.hpp"
#include <cstdint>
#include <memory>

enum UDTSockType {UDT_STREAM = 1, UDT_DGRAM};

class CUDT
{
friend class CUDTSocket;
friend class CUDTUnited;
friend class CCC;
friend struct CUDTComp;
friend class CCache<CInfoBlock>;
friend class CRendezvousQueue;
friend class CSndQueue;
friend class CRcvQueue;
friend class CSndUList;
friend class CRcvUList;

private: // constructor and desctructor
   CUDT ();
   CUDT (CUDT const & ancestor);
   CUDT const & operator = (CUDT const &) { return *this; }
   ~CUDT();

public: //API
   static int startup (UDT::startup_context && ctx);
   static int cleanup ();
   static UDTSOCKET socket(int af, int type = SOCK_STREAM, int protocol = 0);
   static int bind(UDTSOCKET u, const sockaddr* name, int namelen);
   static int bind(UDTSOCKET u, UDPSOCKET udpsock);
   static int listen(UDTSOCKET u, int backlog);
   static UDTSOCKET accept(UDTSOCKET u, sockaddr* addr, int* addrlen);
   static int connect(UDTSOCKET u, const sockaddr* name, int namelen);
   static int close(UDTSOCKET u);
   static int getpeername(UDTSOCKET u, sockaddr* name, int* namelen);
   static int getsockname(UDTSOCKET u, sockaddr* name, int* namelen);
   static int getsockopt(UDTSOCKET u, int level, UDTOpt optname, void* optval, int* optlen);
   static int setsockopt(UDTSOCKET u, int level, UDTOpt optname, const void* optval, int optlen);
   static int send (UDTSOCKET u, const char * buf, int len, int flags);
   static int recv (UDTSOCKET u, char * buf, int len, int flags);
   static int sendmsg (UDTSOCKET u, const char * buf, int len, int ttl = -1, bool inorder = false);
#if NETTY__UDT_PATCHED
   static int recvmsg (UDTSOCKET u, char * buf, int len, bool * pHaveMsgStill = nullptr);
#else
   static int recvmsg (UDTSOCKET u, char * buf, int len);
#endif
   static int sendfile (UDTSOCKET u, std::fstream & ifs, int & offset, int size, int block = 364000);
   static int recvfile (UDTSOCKET u, std::fstream & ofs, int & offset, int size, int block = 7280000);
   static int select(int nfds, ud_set* readfds, ud_set* writefds, ud_set* exceptfds, const timeval* timeout);
   static int selectEx(const std::vector<UDTSOCKET>& fds, std::vector<UDTSOCKET>* readfds, std::vector<UDTSOCKET>* writefds, std::vector<UDTSOCKET>* exceptfds, int64_t msTimeOut);
   static int epoll_create();
   static int epoll_add_usock(const int eid, const UDTSOCKET u, const int* events = nullptr);
   static int epoll_add_ssock(const int eid, const SYSSOCKET s, const int* events = nullptr);
   static int epoll_remove_usock(const int eid, const UDTSOCKET u);
   static int epoll_remove_ssock(const int eid, const SYSSOCKET s);
   static int epoll_wait(const int eid, std::set<UDTSOCKET>* readfds, std::set<UDTSOCKET>* writefds, int64_t msTimeOut, std::set<SYSSOCKET>* lrfds = NULL, std::set<SYSSOCKET>* wrfds = NULL);
   static int epoll_release(const int eid);
   static CUDTException& getlasterror();
   static int perfmon(UDTSOCKET u, CPerfMon* perf, bool clear = true);
   static UDTSTATUS getsockstate(UDTSOCKET u);

public: // internal API
   static CUDT* getUDTHandle(UDTSOCKET u);

private:
      // Functionality:
      //    initialize a UDT entity and bind to a local address.
      // Parameters:
      //    None.
      // Returned value:
      //    None.

   void open();

      // Functionality:
      //    Start listening to any connection request.
      // Parameters:
      //    None.
      // Returned value:
      //    None.

   void listen();

      // Functionality:
      //    Connect to a UDT entity listening at address "peer".
      // Parameters:
      //    0) [in] peer: The address of the listening UDT entity.
      // Returned value:
      //    None.

   void connect(const sockaddr* peer);

      // Functionality:
      //    Process the response handshake packet.
      // Parameters:
      //    0) [in] pkt: handshake packet.
      // Returned value:
      //    Return 0 if connected, positive value if connection is in progress, otherwise error code.

   int connect(const CPacket& pkt);// throw ();

      // Functionality:
      //    Connect to a UDT entity listening at address "peer", which has sent "hs" request.
      // Parameters:
      //    0) [in] peer: The address of the listening UDT entity.
      //    1) [in/out] hs: The handshake information sent by the peer side (in), negotiated value (out).
      // Returned value:
      //    None.

   void connect(const sockaddr* peer, CHandShake* hs);

      // Functionality:
      //    Close the opened UDT entity.
      // Parameters:
      //    None.
      // Returned value:
      //    None.

   void close();

      // Functionality:
      //    Request UDT to send out a data block "data" with size of "len".
      // Parameters:
      //    0) [in] data: The address of the application data to be sent.
      //    1) [in] len: The size of the data block.
      // Returned value:
      //    Actual size of data sent.

   int send(const char* data, int len);

      // Functionality:
      //    Request UDT to receive data to a memory block "data" with size of "len".
      // Parameters:
      //    0) [out] data: data received.
      //    1) [in] len: The desired size of data to be received.
      // Returned value:
      //    Actual size of data received.

   int recv (char * data, int len);

      // Functionality:
      //    send a message of a memory block "data" with size of "len".
      // Parameters:
      //    0) [out] data: data received.
      //    1) [in] len: The desired size of data to be received.
      //    2) [in] ttl: the time-to-live of the message.
      //    3) [in] inorder: if the message should be delivered in order.
      // Returned value:
      //    Actual size of data sent.
   int sendmsg(const char * data, int len, int ttl, bool inorder);

      // Functionality:
      //    Receive a message to buffer "data".
      // Parameters:
      //    0) [out] data: data received.
      //    1) [in] len: size of the buffer.
      // Returned value:
      //    Actual size of data received.

#if NETTY__UDT_PATCHED
   int recvmsg (char * data, int len, bool * pHaveMsgStill = nullptr);
#else
   int recvmsg (char * data, int len);
#endif
      // Functionality:
      //    Request UDT to send out a file described as "fd", starting from "offset", with size of "size".
      // Parameters:
      //    0) [in] ifs: The input file stream.
      //    1) [in, out] offset: From where to read and send data; output is the new offset when the call returns.
      //    2) [in] size: How many data to be sent.
      //    3) [in] block: size of block per read from disk
      // Returned value:
      //    Actual size of data sent.

   int sendfile (std::fstream& ifs, int & offset
      , int size, int block = 366000);

      // Functionality:
      //    Request UDT to receive data into a file described as "fd", starting from "offset", with expected size of "size".
      // Parameters:
      //    0) [out] ofs: The output file stream.
      //    1) [in, out] offset: From where to write data; output is the new offset when the call returns.
      //    2) [in] size: How many data to be received.
      //    3) [in] block: size of block per write to disk
      // Returned value:
      //    Actual size of data received.

   int recvfile (std::fstream & ofs, int & offset
      , int size, int block = 7320000);

      // Functionality:
      //    Configure UDT options.
      // Parameters:
      //    0) [in] optName: The enum name of a UDT option.
      //    1) [in] optval: The value to be set.
      //    2) [in] optlen: size of "optval".
      // Returned value:
      //    None.

   void setOpt(UDTOpt optName, const void* optval, int optlen);

      // Functionality:
      //    Read UDT options.
      // Parameters:
      //    0) [in] optName: The enum name of a UDT option.
      //    1) [in] optval: The value to be returned.
      //    2) [out] optlen: size of "optval".
      // Returned value:
      //    None.

   void getOpt(UDTOpt optName, void* optval, int& optlen);

      // Functionality:
      //    read the performance data since last sample() call.
      // Parameters:
      //    0) [in, out] perf: pointer to a CPerfMon structure to record the performance data.
      //    1) [in] clear: flag to decide if the local performance trace should be cleared.
      // Returned value:
      //    None.

   void sample(CPerfMon* perf, bool clear = true);

private:
   static CUDTUnited s_UDTUnited;               // UDT global management base

public:
   static const UDTSOCKET INVALID_SOCK;         // invalid socket descriptor
   static const int ERROR;                      // socket api error returned value

private: // Identification
   UDTSOCKET m_SocketID;                        // UDT socket number
   UDTSockType m_iSockType;                     // Type of the UDT connection (SOCK_STREAM or SOCK_DGRAM)
   UDTSOCKET m_PeerID;                          // peer id, for multiplexer
   static const int m_iVersion;                 // UDT version, for compatibility use

private: // Packet sizes
   int m_iPktSize;                              // Maximum/regular packet size, in bytes
   int m_iPayloadSize;                          // Maximum/regular payload size, in bytes

private: // Options
    int m_iMSS {1500};            // Maximum Segment Size, in bytes.
    bool m_bSynSending {true};    // Sending syncronization mode.
    bool m_bSynRecving {true};    // Receiving syncronization mode.
    int m_iFlightFlagSize {25600};// Maximum number of packets in flight from
                                  // the peer side.
    int m_iSndBufSize {8192};     // Maximum UDT sender buffer size.
    int m_iRcvBufSize {8192};     // Maximum UDT receiver buffer size.
                                  // Rcv buffer MUST NOT be bigger than Flight
                                  // Flag size.
    linger m_Linger {1, 180};     // Linger information on close.
    int m_iUDPSndBufSize {65536}; // UDP sending buffer size.
    int m_iUDPRcvBufSize;         // UDP receiving buffer size.
    int m_iIPversion;             // IP version.
    bool m_bRendezvous {false};   // Rendezvous connection mode
    int m_iSndTimeOut {-1};       // Sending timeout in milliseconds
    int m_iRcvTimeOut {-1};       // Receiving timeout in milliseconds
    bool m_bReuseAddr {true};     // Reuse an exiting port or not, for UDP
                                  // multiplexer. This must be true, because all
                                  // accepted sockets shared the same port with
                                  // the listener.
    int64_t m_llMaxBW {-1};       // Maximum data transfer rate (threshold).

private: // congestion control
   CCCVirtualFactory* m_pCCFactory {nullptr};   // Factory class to create a specific CC instance
   CCC * m_pCC {nullptr};                       // congestion control class
   CCache<CInfoBlock>* m_pCache {nullptr};      // network information cache

private: // Status
   bool m_bListening {false};   // If the UDT entit is listening to connection
   bool m_bConnecting {false};  // The short phase when connect() is called but not yet completed
   bool m_bConnected {false};   // Whether the connection is on or off
   bool m_bClosing {false};     // If the UDT entity is closing
   bool m_bShutdown {false};    // If the peer side has shutdown the connection
   bool m_bBroken {false};      // If the connection has been broken
   bool m_bPeerHealth {true};   // If the peer status is normal
   bool m_bOpened {false};      // If the UDT entity has been opened
   int m_iBrokenCounter;        // a counter (number of GC checks) to let the GC tag this socket as disconnected

   int m_iEXPCount;             // Expiration counter
   int m_iEXPMaxCounter {16};   // Expiration maximum counter
   std::uint64_t m_ullEXPThreshold {5000000}; // Base expiration threshold

   int m_iBandwidth;            // Estimated bandwidth, number of packets per second
   int m_iRTT;                  // RTT, in microseconds
   int m_iRTTVar;               // RTT variance
   int m_iDeliveryRate;         // Packet arrival rate at the receiver side

   uint64_t m_ullLingerExpiration {0}; // Linger expiration time (for GC to close a socket with data in sending buffer)

   CHandShake m_ConnReq;        // connection request
   CHandShake m_ConnRes;        // connection response
   int64_t m_llLastReqTime {0}; // last time when a connection request is sent

private: // Sending related data
   std::unique_ptr<CSndBuffer> m_pSndBuffer;         // Sender buffer
   std::unique_ptr<CSndLossList> m_pSndLossList;     // Sender loss list
   std::unique_ptr<CPktTimeWindow> m_pSndTimeWindow; // Packet sending time window

   volatile uint64_t m_ullInterval;             // Inter-packet time, in CPU clock cycles
   uint64_t m_ullTimeDiff;                      // aggregate difference in inter-packet time

   volatile int m_iFlowWindowSize;              // Flow control window size
   volatile double m_dCongestionWindow;         // congestion window size

   volatile int32_t m_iSndLastAck;              // Last ACK received
   volatile int32_t m_iSndLastDataAck;          // The real last ACK that updates the sender buffer and loss list
   volatile int32_t m_iSndCurrSeqNo;            // The largest sequence number that has been sent
   int32_t m_iLastDecSeq;                       // Sequence number sent last decrease occurs
   int32_t m_iSndLastAck2;                      // Last ACK2 sent back
   uint64_t m_ullSndLastAck2Time;               // The time when last ACK2 was sent back

   int32_t m_iISN;                              // Initial Sequence Number

   void CCUpdate();

private: // Receiving related data
   std::unique_ptr<CRcvBuffer> m_pRcvBuffer;         // Receiver buffer
   std::unique_ptr<CRcvLossList> m_pRcvLossList;     // Receiver loss list
   std::unique_ptr<CACKWindow> m_pACKWindow;         // ACK history window
   std::unique_ptr<CPktTimeWindow> m_pRcvTimeWindow; // Packet arrival time window

   int32_t m_iRcvLastAck;                        // Last sent ACK
   uint64_t m_ullLastAckTime;                    // Timestamp of last ACK
   int32_t m_iRcvLastAckAck;                     // Last sent ACK that has been acknowledged
   int32_t m_iAckSeqNo;                          // Last ACK sequence number
   int32_t m_iRcvCurrSeqNo;                      // Largest received sequence number
   uint64_t m_ullLastWarningTime;                // Last time that a warning message is sent
   int32_t m_iPeerISN;                           // Initial Sequence Number of the peer side

private: // synchronization: mutexes and conditions
   pthread_mutex_t m_ConnectionLock;            // used to synchronize connection operation

   pthread_cond_t m_SendBlockCond;              // used to block "send" call
   pthread_mutex_t m_SendBlockLock;             // lock associated to m_SendBlockCond

   pthread_mutex_t m_AckLock;                   // used to protected sender's loss list when processing ACK

   pthread_cond_t m_RecvDataCond;               // used to block "recv" when there is no data
   pthread_mutex_t m_RecvDataLock;              // lock associated to m_RecvDataCond

   pthread_mutex_t m_SendLock;                  // used to synchronize "send" call
   pthread_mutex_t m_RecvLock;                  // used to synchronize "recv" call

   void initSynch();
   void destroySynch();
   void releaseSynch();

private: // Generation and processing of packets
   void sendCtrl(int pkttype, void* lparam = NULL, void* rparam = NULL, int size = 0);
   void processCtrl(CPacket& ctrlpkt);
   int packData(CPacket& packet, uint64_t& ts);
   int processData(CUnit* unit);
   int listen(sockaddr* addr, CPacket& packet);

private: // Trace
   uint64_t m_StartTime;           // timestamp when the UDT entity is started
   int64_t m_llSentTotal;          // total number of sent data packets, including retransmissions
   int64_t m_llRecvTotal;          // total number of received packets
   int m_iSndLossTotal;            // total number of lost packets (sender side)
   int m_iRcvLossTotal;            // total number of lost packets (receiver side)
   int m_iRetransTotal;            // total number of retransmitted packets
   int m_iSentACKTotal;            // total number of sent ACK packets
   int m_iRecvACKTotal;            // total number of received ACK packets
   int m_iSentNAKTotal;            // total number of sent NAK packets
   int m_iRecvNAKTotal;            // total number of received NAK packets
   int64_t m_llSndDurationTotal;   // total real time for sending

   uint64_t m_LastSampleTime;      // last performance sample time
   int64_t m_llTraceSent;          // number of pakctes sent in the last trace interval
   int64_t m_llTraceRecv;          // number of pakctes received in the last trace interval
   int m_iTraceSndLoss;            // number of lost packets in the last trace interval (sender side)
   int m_iTraceRcvLoss;            // number of lost packets in the last trace interval (receiver side)
   int m_iTraceRetrans;            // number of retransmitted packets in the last trace interval
   int m_iSentACK;                 // number of ACKs sent in the last trace interval
   int m_iRecvACK;                 // number of ACKs received in the last trace interval
   int m_iSentNAK;                 // number of NAKs sent in the last trace interval
   int m_iRecvNAK;                 // number of NAKs received in the last trace interval
   int64_t m_llSndDuration;        // real time for sending
   int64_t m_llSndDurationCounter; // timers to record the sending duration

private: // Timers
   uint64_t m_ullCPUFrequency;                  // CPU clock frequency, used for Timer, ticks per microsecond

   static const int m_iSYNInterval;             // Periodical Rate Control Interval, 10000 microsecond
   static const int m_iSelfClockInterval;       // ACK interval for self-clocking

   uint64_t m_ullNextACKTime;          // Next ACK time, in CPU clock cycles, same below
   uint64_t m_ullNextNAKTime;          // Next NAK time

   volatile uint64_t m_ullSYNInt;      // SYN interval
   volatile uint64_t m_ullACKInt;      // ACK interval
   volatile uint64_t m_ullNAKInt;      // NAK interval
   volatile uint64_t m_ullLastRspTime; // time stamp of last response from the peer

   uint64_t m_ullMinNakInt;            // NAK timeout lower bound; too small value can cause unnecessary retransmission
   uint64_t m_ullMinExpInt;            // timeout lower bound threshold: too small timeout can cause problem

   int m_iPktCount;                    // packet counter for ACK
   int m_iLightACKCount;               // light ACK counter

   uint64_t m_ullTargetTime;           // scheduled time of next packet sending

private: // for UDP multiplexer
   CSndQueue* m_pSndQueue {nullptr}; // packet sending queue
   CRcvQueue* m_pRcvQueue {nullptr}; // packet receiving queue
   sockaddr* m_pPeerAddr {nullptr};  // peer address
   uint32_t m_piSelfIP[4];           // local UDP IP address
   CSNode* m_pSNode {nullptr};       // node information for UDT list used in snd queue
   CRNode* m_pRNode {nullptr};       // node information for UDT list used in rcv queue

private: // for epoll
   std::set<int> m_sPollID;                     // set of epoll ID to trigger

private:
   void checkTimers ();
   void addEPoll (int const eid);
   void removeEPoll (int const eid);
};
