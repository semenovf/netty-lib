////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2022 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2021.09.10 Initial version
////////////////////////////////////////////////////////////////////////////////
#include <iostream>
#include <string>

extern void server ();
extern void client ();

static void usage (std::string const & program, std::string const & error_string)
{
    std::cerr << error_string
        << "\nRun `" << program << " client | server`"
        << std::endl;
}

int main (int argc, char * argv[])
{
    std::string program{argv[0]};

    if (argc < 2) {
        usage(program, "Too few arguments");
        return EXIT_FAILURE;
    }

    if (std::string{"server"} == argv[1]) {
        server();
    } else if (std::string{"client"} == argv[1]) {
        client();
    } else {
        usage(program, "Bad argument");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

