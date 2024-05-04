////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.05.01 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "reliable_delivery.hpp"
#include "pfs/argvapi.hpp"
#include "pfs/emitter.hpp"
#include "pfs/memory.hpp"
#include "pfs/standard_paths.hpp"
#include "pfs/netty/socket4_addr.hpp"
#include <chrono>
#include <cstdlib>

using namespace std::chrono;

bool __failure = false;

void print_usage (pfs::filesystem::path const & programName
    , std::string const & errorString = std::string{})
{
    std::FILE * out = stdout;

    if (!errorString.empty()) {
        out = stderr;
        fmt::println(out, "Error: {}", errorString);
    }

    fmt::println(out, "Usage:\n\n"
        "{0} --help | -h\n"
        "\tPrint this help and exit\n\n"
        "{0} [--host-id=HOST_ID] [--listener=ADDR:PORT] --peer=ADDR:PORT...\n\n"
        "--host-id=HOST_ID\n"
        "\tSet host ID. If not specified then it will be generated automatically\n"
        "--listener=ADDR:PORT\n"
        "\tSpecify listener socket address. Default is 0.0.0.0:42042\n"
        "--discovery=ADDR:PORT\n"
        "\tSpecify discovery receiver socket address. Default is 0.0.0.0:42043\n"
        "--peer=ADDR\n"
        "\tSpecify peer socket addresses for communications\n"
        , programName);
}

int main (int argc, char * argv[])
{
    netty::p2p::peer_id host_id;
    std::vector<netty::inet4_addr> peers;
    netty::socket4_addr discovery_saddr {netty::inet4_addr::any_addr_value, 42043};
    netty::socket4_addr listener_saddr {netty::inet4_addr::any_addr_value, 42042};

    auto commandLine = pfs::make_argvapi(argc, argv);
    auto programName = commandLine.program_name();
    auto commandLineIterator = commandLine.begin();

    if (commandLineIterator.has_more()) {
        while (commandLineIterator.has_more()) {
            auto x = commandLineIterator.next();

            if (x.is_option("help") || x.is_option("h")) {
                print_usage(programName);
                return EXIT_SUCCESS;
            } else if (x.is_option("host-id")) {
                if (!x.has_arg()) {
                    print_usage(programName, "Expected host ID");
                    return EXIT_FAILURE;
                }

                host_id = pfs::from_string<netty::p2p::peer_id>(pfs::to_string(x.arg()));

                if (host_id == netty::p2p::peer_id{}) {
                    fmt::println(stderr, "Bad host ID");
                    return EXIT_FAILURE;
                }
            } else if (x.is_option("listener")) {
                if (!x.has_arg()) {
                    print_usage(programName, "Expected listener");
                    return EXIT_FAILURE;
                }

                auto saddr = netty::socket4_addr::parse(x.arg());

                if (!saddr) {
                    fmt::println(stderr, "Bad listener address");
                    return EXIT_FAILURE;
                }

                listener_saddr = std::move(*saddr);
            } else if (x.is_option("discovery")) {
                if (!x.has_arg()) {
                    print_usage(programName, "Expected discovery");
                    return EXIT_FAILURE;
                }

                auto saddr = netty::socket4_addr::parse(x.arg());

                if (!saddr) {
                    fmt::println(stderr, "Bad discovery address");
                    return EXIT_FAILURE;
                }

                discovery_saddr = std::move(*saddr);
            } else if (x.is_option("peer")) {
                if (!x.has_arg()) {
                    print_usage(programName, "Expected address");
                    return EXIT_FAILURE;
                }

                auto addr = netty::inet4_addr::parse(x.arg());

                if (!addr) {
                    fmt::println(stderr, "Bad peer address");
                    return EXIT_FAILURE;
                }

                peers.push_back(std::move(*addr));
            } else {
                fmt::println(stderr, "Bad arguments. Try --help option.");
                return EXIT_FAILURE;
            }
        }
    }

    if (peers.empty()) {
        fmt::println(stderr, "No peer specified. Try --help option.");
        return EXIT_FAILURE;
    }

    if (host_id == netty::p2p::peer_id{})
        host_id = pfs::generate_uuid();

    discovery_engine::options discoveryengineopts;
    discoveryengineopts.host_port = listener_saddr.port;
    auto discoveryengine = pfs::make_unique<discovery_engine>(host_id, std::move(discoveryengineopts));

    reliable_delivery_engine::options deliveryengineopts;
    deliveryengineopts.listener_saddr = listener_saddr;
    deliveryengineopts.listener_backlog = 99;

    auto storage = pfs::make_unique<persistent_storage>(pfs::filesystem::standard_paths::temp_folder());
    auto deliveryengine = pfs::make_unique<reliable_delivery_engine>(std::move(storage), host_id
        , std::move(deliveryengineopts));

    std::vector<netty::p2p::peer_id> defered_expired_peers;
    pfs::emitter<netty::p2p::peer_id, std::string> sendStringData;
    pfs::emitter<netty::p2p::peer_id, std::vector<char>> sendVectorData;

    sendStringData.connect([& deliveryengine] (netty::p2p::peer_id addressee, std::string data) {
        deliveryengine->enqueue(addressee, std::move(data));
    });

    sendVectorData.connect([& deliveryengine] (netty::p2p::peer_id addressee, std::vector<char> data) {
        deliveryengine->enqueue(addressee, std::move(data));
    });

    discoveryengine->on_failure = [] (netty::error const & err) {
        LOGE("", "Descovery failure: {}, exit", err.what());
        __failure = true;
    };

    discoveryengine->peer_discovered2 = [& deliveryengine] (netty::host4_addr haddr, milliseconds const & /*timediff*/) {
        LOGD("", "Peer discovered, connecting: {}", to_string(haddr));
        deliveryengine->connect(std::move(haddr));
    };

    discoveryengine->peer_timediff = [] (netty::p2p::peer_id peerid, milliseconds const & timediff) {
        LOGD("", "Peer time difference: {}: {}", peerid, timediff);
    };

    discoveryengine->peer_expired2 = [& deliveryengine] (netty::host4_addr haddr) {
        LOGD("", "Peer expired: {}", to_string(haddr));
        deliveryengine->release_peer(haddr.host_id);
    };

    deliveryengine->on_failure = [] (netty::error const & err) {
        LOGE("", "Unrecoverable error: {}, exit", err.what());
        __failure = true;
    };

    deliveryengine->on_error = [] (std::string const & errstr) {
        LOGE("", "{}", errstr);
    };

    deliveryengine->on_warn = [] (std::string const & errstr) {
        LOGW("", "{}", errstr);
    };

    deliveryengine->writer_ready = [] (netty::host4_addr haddr) {
        LOGD("", "Writer ready: {}", to_string(haddr));
    };

    deliveryengine->writer_closed = [] (netty::host4_addr haddr) {
        LOGD("", "Writer ready: {}", to_string(haddr));
    };

    deliveryengine->reader_ready = [] (netty::host4_addr haddr) {
        LOGD("", "Reader ready: {}", to_string(haddr));
    };

    deliveryengine->reader_closed = [] (netty::host4_addr haddr) {
        LOGD("", "Reader closed: {}", to_string(haddr));
    };

    deliveryengine->defere_expire_peer = [& discoveryengine] (netty::p2p::peer_id peer_id) {
        discoveryengine->expire_peer(peer_id);
    };

    deliveryengine->channel_established = [& sendStringData] (netty::host4_addr haddr) {
        LOGD("", "Channel established: {}", to_string(haddr));

        sendStringData(haddr.host_id, "HELLO");
    };

    deliveryengine->channel_closed = [] (netty::p2p::peer_id peer_id) {
        LOGD("", "Channel closed: {}", peer_id);
    };

    deliveryengine->data_received = [& sendVectorData] (netty::p2p::peer_id peer_id, std::vector<char> data) {
        LOGD("", "Message received from: {}", peer_id);
        sendVectorData(peer_id, std::move(data));
    };

    discoveryengine->add_receiver(discovery_saddr);

    for (auto const & peer: peers) {
        discoveryengine->add_target(netty::socket4_addr{peer, discovery_saddr.port}, seconds{1});
        LOGD("", "Peer added as discovery target: {}", to_string(peer));
    }

    milliseconds timeout {10};

    deliveryengine->ready();

    while (!__failure) {
        auto t = discoveryengine->step_timing();
        deliveryengine->step(timeout > duration_cast<milliseconds>(t)
            ? timeout - duration_cast<milliseconds>(t) : milliseconds{0});
    }

    discoveryengine->expire_all_peers();

    return EXIT_SUCCESS;
}
