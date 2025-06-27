////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.06.27 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "tag.hpp"
#include <pfs/netty/inet4_addr.hpp>
#include <pfs/argvapi.hpp>
#include <pfs/filesystem.hpp>
#include <pfs/log.hpp>

namespace fs = pfs::filesystem;

static void print_usage (fs::path const & programName, std::string const & errorString = std::string{})
{
    if (!errorString.empty())
        LOGE(TAG, "{}", errorString);

    fmt::println("Usage:\n\n"
        "{0} --help | -h\n"
        "{0} DOMAIN_NAME..."
        , programName);
}

int main (int argc, char * argv[])
{
    auto commandLine = pfs::make_argvapi(argc, argv);
    auto programName = commandLine.program_name();
    auto commandLineIterator = commandLine.begin();

    if (commandLineIterator.has_more()) {
        while (commandLineIterator.has_more()) {
            auto x = commandLineIterator.next();

            if (x.is_option("help") || x.is_option("h")) {
                print_usage(programName);
                return EXIT_SUCCESS;
            } else {
                auto domain_name = x.arg();
                netty::error err;

                fmt::println("Domain name: {}", domain_name);

                auto addresses = netty::inet4_addr::resolve(to_string(domain_name), & err);

                if (err) {
                    fmt::println(stderr, "\tResolution failure: {}", err.what());
                } else {
                    for (auto const & a: addresses)
                        fmt::println("\t{}", to_string(a));
                }
            }
        }
    } else {
        print_usage(programName);
        return EXIT_SUCCESS;
    }

    return EXIT_SUCCESS;
}
