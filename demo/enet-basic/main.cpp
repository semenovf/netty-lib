////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.05.11 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "pfs/endian.hpp"
#include "pfs/emitter.hpp"
#include "pfs/log.hpp"
#include "pfs/string_view.hpp"
#include "pfs/netty/socket4_addr.hpp"
#include "linenoise/linenoise.h"
#include <enet/enet.h>
#include <atomic>
#include <condition_variable>
#include <chrono>
#include <cstdlib>
#include <mutex>
#include <thread>
#include <vector>

using pfs::string_view;

struct ClientCommands
{
    pfs::emitter_mt<netty::socket4_addr> connectServer;
    // pfs::emitter_mt<> disconnect_service;
    // pfs::emitter_mt<std::vector<char>> send;
};

static std::atomic_bool sFinishFlag {false};
static std::atomic_bool sConnectedFlag {false};

std::vector<char> peerData {'1', '2', '3', '4', 0};

void step (ENetHost * host, std::chrono::milliseconds timeout)
{
    ENetEvent event;
    auto rc = enet_host_service(host, & event, timeout.count());

    if (rc > 0) {
        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT:
                LOGD("", "A new client connected from {}:{}"
                    , event.peer->address.host, event.peer->address.port);

                // Store any relevant client information here
                event.peer->data = peerData.data();
                break;

            case ENET_EVENT_TYPE_RECEIVE:
                LOGD("", "A packet of length {} containing \\{\\} was received from {} on channel {}"
                    , event.packet->dataLength/*, event.packet->data*/
                    , static_cast<char const *>(event.peer->data), event.channelID);

                // Clean up the packet now that we're done using it
                enet_packet_destroy(event.packet);
                break;

            case ENET_EVENT_TYPE_DISCONNECT:
                LOGD("", "{} disconnected", static_cast<char const *>(event.peer->data));

                // Reset the peer's client information
                event.peer->data = nullptr;

            case ENET_EVENT_TYPE_NONE:
            default:
                LOGD("", "No event");
                break;
        }
    }
}

void completion (char const * buf, linenoiseCompletions * lc)
{
    if (buf[0] == '/') {
        if (buf[1] == 'e')
            linenoiseAddCompletion(lc,"/exit");
        else if (buf[1] == 'q')
            linenoiseAddCompletion(lc,"/quit");
    }
}

char * hints (const char * buf, int * color, int * bold)
{
    if (strcasecmp(buf, "hello") == 0) {
        *color = 35;
        *bold = 0;
        return const_cast<char *>(" World");
    }

    return nullptr;
}

int main (int /*argc*/, char * /*argv*/[])
{
    netty::socket4_addr listener_saddr {netty::inet4_addr{127, 0, 0, 1}, 42142};
    auto success = enet_initialize() == 0;

    if (!success) {
        LOGE("", "ENet initialization failure");
        return EXIT_FAILURE;
    }

    std::mutex ready_mtx;
    std::condition_variable ready_cv;
    std::unique_lock<std::mutex> ready_lk(ready_mtx);

    std::thread serverThread {
        [& ready_cv, listener_saddr] () {
            /* Bind the server to the default localhost.     */
            /* A specific host address can be specified by   */
            /* enet_address_set_host (& address, "x.x.x.x"); */

            ENetAddress address;
            address.host = pfs::to_network_order(static_cast<enet_uint32>(listener_saddr.addr));
            address.port = listener_saddr.port; // Bind the server to port

            ENetHost * server = enet_host_create (& address // the address to bind the server host to
                , 32   // allow up to 32 clients and/or outgoing connections
                ,  2   // allow up to 2 channels to be used, 0 and 1
                ,  0   // assume any amount of incoming bandwidth
                ,  0); // assume any amount of outgoing bandwidth

            LOGD("", "Service ready: {}", (server != nullptr));
            ready_cv.notify_one();

            if (server == nullptr) {
                LOGE("", "An error occurred while trying to create an ENet server host.");
                sFinishFlag = true;
                return;
            }

            while (!sFinishFlag) {
                step(server, std::chrono::milliseconds{1000});
            }

            enet_host_destroy(server);
        }
    };

    ClientCommands cmd;

    std::thread clientThread {
        [& ready_cv, & cmd] () {
            ENetHost * c = enet_host_create (nullptr // create a client host
                , 1   // only allow 1 outgoing connection
                , 2   // allow up 2 channels to be used, 0 and 1
                , 0   // assume any amount of incoming bandwidth
                , 0); // assume any amount of outgoing bandwidth

            LOGD("", "Client ready: {}", (c != nullptr));
            ready_cv.notify_one();

            if (c == nullptr) {
                LOGE("", "An error occurred while trying to create an ENet client host.");
                sFinishFlag = true;
                return;
            }

            cmd.connectServer.connect([c, & ready_cv] (netty::socket4_addr saddr) {
                ENetAddress address;

                address.host = pfs::to_network_order(static_cast<enet_uint32>(saddr.addr));
                address.port = saddr.port;

                // Initiate the connection, allocating the two channels 0 and 1
                ENetPeer * peer = enet_host_connect(c, & address, 2, 0);

                if (peer == nullptr) {
                    LOGE("", "No available peers for initiating an ENet connection");
                    ready_cv.notify_one();
                    return;
                }

//                 /* Wait up to 5 seconds for the connection attempt to succeed. */
//                 if (enet_host_service (client, & event, 5000) > 0 &&
//                     event.type == ENET_EVENT_TYPE_CONNECT)
//                 {
//                     puts ("Connection to some.server.net:1234 succeeded.");
//                     ...
//                     ...
//                     ...
//                 }
//                 else
//                 {
//                     /* Either the 5 seconds are up or a disconnect event was */
//                     /* received. Reset the peer in the event the 5 seconds   */
//                     /* had run out without any significant event.            */
//                     enet_peer_reset (peer);
//
//                     puts ("Connection to some.server.net:1234 failed.");
//                 }
            });

            while (!sFinishFlag) {
                step(c, std::chrono::milliseconds{1000});
            }

            // FIXME
            //enet_peer_reset(peer);
            enet_host_destroy(c);
        }
    };

    ready_cv.wait(ready_lk);

    LOGD("", "Server and client threads ready: {}", !sFinishFlag.load());

    linenoiseSetCompletionCallback(completion);
    linenoiseSetHintsCallback(hints);

    while (!sFinishFlag) {
        auto line = linenoise("client> ");

        if (line == nullptr) {
            sFinishFlag = true;
            break;
        }

        string_view sv {line};

        if (sv == "/e" || sv == "/q" || sv == "/exit" || sv == "/quit") {
            sFinishFlag = true;
            break;
        }

        fmt::print("\r");

        if (sv == "connect") {
            if (!sConnectedFlag) {
                cmd.connectServer(listener_saddr);
                ready_cv.wait(ready_lk);
            } else {
                LOGW("", "Already connected");
            }
        } else if (sv == "disconnect") {
            // if (sConnectedFlag) {
            //     cmd.disconnect_service();
            //     ready_cv.wait(ready_lk);
            // } else {
            //     LOGW("", "Already disconnected");
            // }
        } else if (sv == "echo") {
            // message_serializer_t ms {echo {"Hello, world!"}};
            // service_t::client::output_envelope_type env {message_enum::echo, ms.take()};
            // cmd.send(env.take());
            // ready_cv.wait(ready_lk);
        }
    }

    serverThread.join();
    clientThread.join();

    enet_deinitialize();

    return EXIT_SUCCESS;
}
