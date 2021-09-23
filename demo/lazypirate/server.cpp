////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.09.10 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "../zhelpers.hpp"

//
// Source: https://github.com/booksbyus/zguide/blob/master/examples/C%2B%2B/lpserver.cpp
//
// Lazy Pirate server
// Binds REQ socket to tcp://*:5555
// Like hwserver except:
// - echoes request as-is
// - randomly runs slowly, or exits to simulate a crash.
//

void server ()
{
//     srandom((unsigned) time (NULL));

    zmq::context_t context(1);
    zmq::socket_t server(context, ZMQ_REP);
    server.bind("tcp://*:5555");

    int cycles = 0;

    while (true) {
        std::string request = s_recv(server);
        cycles++;

        // Simulate various problems, after a few cycles
        if (cycles > 3 && within(3) == 0) {
            std::cout << "I: simulating a crash" << std::endl;
            break;
        } else if (cycles > 3 && within(3) == 0) {
            std::cout << "I: simulating CPU overload" << std::endl;
            sleep_for_seconds(2);
        }

        std::cout << "I: normal request (" << request << ")" << std::endl;
        sleep_for_seconds(1); // Do some heavy work
        s_send(server, request);
    }
}

