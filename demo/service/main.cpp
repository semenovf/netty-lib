////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.05.05 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "service.hpp"
#include "process.hpp"
#include "pfs/argvapi.hpp"
#include "pfs/function_queue.hpp"
#include "pfs/emitter.hpp"
#include "pfs/filesystem.hpp"
#include "pfs/i18n.hpp"
#include "pfs/log.hpp"
#include "pfs/string_view.hpp"
#include "linenoise/linenoise.h"
#include <atomic>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

using pfs::string_view;

struct client_commands
{
    pfs::emitter_mt<netty::socket4_addr> connect_service;
    pfs::emitter_mt<> disconnect_service;
    pfs::emitter_mt<std::vector<char>> send;
};

static std::atomic_bool finish_flag {false};
static std::atomic_bool s_connected_flag {false};

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
        "{0} [--listener=ADDR:PORT]\n\n"
        "--listener=ADDR:PORT\n"
        "\tSpecify listener socket address. Default is 0.0.0.0:42142\n"
        , programName);
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

int main (int argc, char * argv[])
{
    //netty::socket4_addr listener_saddr {netty::inet4_addr::any_addr_value, 42142};
    netty::socket4_addr listener_saddr {netty::inet4_addr{127, 0, 0, 1}, 42142};

    auto commandLine = pfs::make_argvapi(argc, argv);
    auto programName = commandLine.program_name();
    auto commandLineIterator = commandLine.begin();

    if (commandLineIterator.has_more()) {
        while (commandLineIterator.has_more()) {
            auto x = commandLineIterator.next();

            if (x.is_option("help") || x.is_option("h")) {
                print_usage(programName);
                return EXIT_SUCCESS;
            } else if (x.is_option("listener")) {
                if (!x.has_arg()) {
                    print_usage(programName, "Expected listener address");
                    return EXIT_FAILURE;
                }

                auto saddr = netty::socket4_addr::parse(x.arg());

                if (!saddr) {
                    fmt::println(stderr, "Bad listener address");
                    return EXIT_FAILURE;
                }

                listener_saddr = std::move(*saddr);
            } else {
                fmt::println(stderr, "Bad arguments. Try --help option.");
                return EXIT_FAILURE;
            }
        }
    }

    std::mutex ready_mtx;
    std::condition_variable ready_cv;
    std::unique_lock<std::mutex> ready_lk(ready_mtx);

    std::thread service_thread {[listener_saddr, & ready_cv] () {
        bool failure = false;
        service_t::server server {listener_saddr};

        server.on_failure = [& failure] (netty::error const & err) {
            LOGE("", "FAILURE: {}", err.what());
            failure = true;
        };

        server.on_error = [] (std::string const & errstr) {
            LOGE("", "{}", errstr);
        };

        server.on_message_received = [& server] (service_t::server::native_socket_type sock
            , service_t::input_envelope_type const & env) {

            server_connection_context conn {& server, sock};
            server_message_processor_t mp;
            auto const & payload = env.payload();
            auto success = mp.parse(conn, env.message_type(), payload.data(), payload.data() + payload.size());

            if (!success) {
                LOGE("", "parse message failure: {}", fmt::underlying(env.message_type()));
                return;
            }
        };

        LOGD("", "Service ready");
        ready_cv.notify_one();

        while (!(failure || finish_flag)) {
            server.step(std::chrono::milliseconds{10});
        }
    }};

    ready_cv.wait(ready_lk);
    client_commands cmd;

    std::thread client_thread {[listener_saddr, & ready_cv, & cmd] () {
        bool failure = false;
        service_t::client c {};
        pfs::function_queue<> command_queue;

        c.on_failure = [& failure, & ready_cv] (netty::error const & err) {
            LOGE("", "Failure on client connection: {}", err.what());
            failure = true;
            ready_cv.notify_one();
        };

        c.on_error = [& ready_cv] (std::string const & errstr) {
            LOGE("", "{}", errstr);
            ready_cv.notify_one();
        };

        c.connected = [listener_saddr, & ready_cv] () {
            LOGD("", "Connected to: {}", to_string(listener_saddr));
            s_connected_flag = true;
            ready_cv.notify_one();
        };

        c.connection_refused = [listener_saddr, & ready_cv] () {
            LOGD("", "Connection refused to: {}", to_string(listener_saddr));
            s_connected_flag = false;
            ready_cv.notify_one();
        };

        c.disconnected = [] () { LOGE("", "Disconnected by the peer"); };

        c.released = [listener_saddr, & ready_cv] () {
            LOGD("", "Disconnected/released from: {}", to_string(listener_saddr));
            s_connected_flag = false;
            ready_cv.notify_one();
        };

        c.on_message_received = [& c, & ready_cv] (service_t::input_envelope_type const & env) {
            client_connection_context conn {& c};
            client_message_processor_t mp;
            auto const & payload = env.payload();
            auto success = mp.parse(conn, env.message_type(), payload.data(), payload.data() + payload.size());

            if (!success) {
                LOGE("", "parse message failure: {}", fmt::underlying(env.message_type()));
            }

            ready_cv.notify_one();
        };

        cmd.connect_service.connect([& c, & command_queue] (netty::socket4_addr listener_saddr) {
            command_queue.push([& c, listener_saddr] { c.connect(listener_saddr); });
        });

        cmd.disconnect_service.connect([& c, & command_queue] () {
            command_queue.push([& c] { c.disconnect(); });
        });

        cmd.send.connect([& c, & command_queue] (std::vector<char> && msg) {
            command_queue.push([& c, msg] {
                LOGD("", "MESSAGE ENQUEUED: size={}", msg.size());
                c.enqueue(msg);
            });
        });

        LOGD("", "Client ready");
        ready_cv.notify_one();

        while (!(failure || finish_flag)) {
            c.step(std::chrono::milliseconds{10});
            command_queue.call_all();
        }
    }};

    ready_cv.wait(ready_lk);

    LOGD("", "Service and client threads ready");
    // LOGD("", "Client connecting...");

    // cmd.connect_service(listener_saddr);

    linenoiseSetCompletionCallback(completion);
    linenoiseSetHintsCallback(hints);

    while (true) {
        auto line = linenoise("client> ");

        if (line == nullptr) {
            finish_flag = true;
            break;
        }

        string_view sv {line};

        if (sv == "/e" || sv == "/q" || sv == "/exit" || sv == "/quit") {
            finish_flag = true;
            break;
        }

        fmt::print("\r");

        if (sv == "connect") {
            if (!s_connected_flag) {
                cmd.connect_service(listener_saddr);
                ready_cv.wait(ready_lk);
            } else {
                LOGW("", "Already connected");
            }
        } else if (sv == "disconnect") {
            if (s_connected_flag) {
                cmd.disconnect_service();
                ready_cv.wait(ready_lk);
            } else {
                LOGW("", "Already disconnected");
            }
        } else if (sv == "echo") {
            message_serializer_t ms {echo {"Hello, world!"}};
            service_t::client::output_envelope_type env {message_enum::echo, ms.take()};
            cmd.send(env.take());
            ready_cv.wait(ready_lk);
        }
    }

    service_thread.join();
    client_thread.join();

    return EXIT_SUCCESS;
}
