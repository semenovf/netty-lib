////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2022 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2021.09.10 Initial version
////////////////////////////////////////////////////////////////////////////////

//
// Source: https://github.com/booksbyus/zguide/blob/master/examples/C%2B%2B/lpclient.cpp
//
//  Lazy Pirate client
//  Use zmq_poll to do a safe request-reply
//  To run, start server and then randomly kill/restart it
//

#include "../zhelpers.hpp"
#include <memory>
#include <sstream>

using zmq_socket_ptr = std::unique_ptr<zmq::socket_t>;

constexpr std::chrono::milliseconds REQUEST_TIMEOUT {2500}; //  msecs, (> 1000!)
constexpr int REQUEST_RETRIES = 3;    //  Before we abandon

zmq_socket_ptr make_client_socket (zmq::context_t & context)
{
    std::cout << "I: connecting to server..." << std::endl;
    zmq_socket_ptr client {new zmq::socket_t(context, ZMQ_REQ)};
    client->connect("tcp://localhost:5555");

    // Configure socket to not wait at close time
    int linger = 0;
    client->set(zmq::sockopt::linger, linger);

    return client;
}

void client ()
{
    zmq::context_t context (1);
    auto client = make_client_socket(context);

    int sequence = 0;
    int retries_left = REQUEST_RETRIES;

    while (retries_left) {
        std::stringstream request;
        request << ++sequence;
        s_send(*client, request.str());

        sleep_for_seconds(1);

        bool expect_reply = true;

        while (expect_reply) {
            // Poll socket for a reply, with timeout
            zmq::pollitem_t items[] = {
                { *client, 0, ZMQ_POLLIN, 0 }
            };

            //zmq::poll(& items[0], 1, REQUEST_TIMEOUT);
            zmq::poll(& items[0], 1, REQUEST_TIMEOUT);

            // If we got a reply, process it
            if (items[0].revents & ZMQ_POLLIN) {
                // We got a reply from the server, must match sequence
                std::string reply = s_recv(*client);

                if (atoi(reply.c_str()) == sequence) {
                    std::cout << "I: server replied OK (" << reply << ")" << std::endl;
                    retries_left = REQUEST_RETRIES;
                    expect_reply = false;
                } else {
                    std::cout << "E: malformed reply from server: " << reply << std::endl;
                }
            } else if (--retries_left == 0) {
                std::cout << "E: server seems to be offline, abandoning" << std::endl;
                expect_reply = false;
                break;
            } else {
                std::cout << "W: no response from server, retrying..." << std::endl;
                client = make_client_socket(context);
                // Send request again, on new socket
                s_send(*client, request.str());
            }
        }
    }
}
