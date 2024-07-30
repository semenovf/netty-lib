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
#include <memory>
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

static std::atomic_bool s_finish_flag {false};
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
        "{0} [--backend=BACKEND] [--listener=ADDR:PORT]\n\n"
        "--backend=BACKEND\n"
        "\tUse one of these backends:\n"
#if NETTY__SELECT_ENABLED
        "\t\t* select\n"
#endif

#if NETTY__POLL_ENABLED
        "\t\t* poll\n"
#endif

#if NETTY__EPOLL_ENABLED
        "\t\t* epoll\n"
#endif

#if NETTY__UDT_ENABLED
        "\t\t* udt\n"
#endif

#if NETTY__ENET_ENABLED
        "\t\t* enet\n"
#endif
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

// template <PollerEnum P>
// inline
// std::shared_ptr<typename service_traits<P>::service_type::respondent::poller_backend_type>
// make_respondent_poller ()
// {
//     return std::shared_ptr<typename service_traits<P>::service_type::respondent::poller_backend_type>{};
// }
//
// template <>
// inline
// std::shared_ptr<typename service_traits<PollerEnum::ENet>::service_type::respondent::poller_backend_type>
// make_respondent_poller<PollerEnum::ENet> ()
// {
//     return std::make_shared<typename service_traits<PollerEnum::ENet>::service_type::respondent::poller_backend_type>();
// }

template <PollerEnum P>
void service_process (netty::socket4_addr listener_saddr, std::condition_variable & ready_cv)
{
    using service_type        = typename service_traits<P>::service_type;
    using respondent_type     = typename service_type::respondent;
    using native_socket_type  = typename respondent_type::native_socket_type;
    using input_envelope_type = typename service_type::input_envelope_type;

    respondent_type respondent {listener_saddr, netty::property_map_t{}};

    respondent.on_failure = [] (netty::error const & err) {
        LOGE("", "FAILURE: {}", err.what());
        s_finish_flag = true;
    };

    respondent.on_error = [] (std::string const & errstr) {
        LOGE("", "{}", errstr);
    };

    respondent.on_message_received = [& respondent] (native_socket_type sock, input_envelope_type const & env) {
        typename service_traits<P>::server_connection_context conn {& respondent, sock};
        typename service_traits<P>::server_message_processor_type mp;
        auto const & payload = env.payload();
        auto success = mp.parse(conn, env.message_type(), payload.data(), payload.data() + payload.size());

        if (!success) {
            LOGE("", "parse message failure: {}", fmt::underlying(env.message_type()));
            return;
        }
    };

    LOGD("", "Service ready");
    ready_cv.notify_one();

    while (!s_finish_flag) {
        respondent.step(std::chrono::milliseconds{10});
    }
}

// template <PollerEnum P>
// inline
// std::shared_ptr<typename service_traits<P>::service_type::requester::poller_backend_type>
// make_requester_poller ()
// {
//     return std::shared_ptr<typename service_traits<P>::service_type::requester::poller_backend_type>{};
// }
//
// template <>
// inline
// std::shared_ptr<typename service_traits<PollerEnum::ENet>::service_type::requester::poller_backend_type>
// make_requester_poller<PollerEnum::ENet> ()
// {
//     return std::make_shared<typename service_traits<PollerEnum::ENet>::service_type::requester::poller_backend_type>();
// }

template <PollerEnum P>
void client_process (client_commands & cmd, netty::socket4_addr listener_saddr, std::condition_variable & ready_cv)
{
    using service_type        = typename service_traits<P>::service_type;
    using requester_type      = typename service_type::requester;
    using input_envelope_type = typename service_type::input_envelope_type;

    requester_type c;
    pfs::function_queue<> command_queue;

    c.on_failure = [& ready_cv] (netty::error const & err) {
        LOGE("", "Failure on requester connection: {}", err.what());
        s_finish_flag = true;
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

    c.on_message_received = [& c, & ready_cv] (input_envelope_type const & env) {
        typename service_traits<P>::client_connection_context conn {& c};
        typename service_traits<P>::client_message_processor_type mp;

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

    cmd.send.connect([& c, & command_queue, & ready_cv] (std::vector<char> && msg) {
        if (c.is_connected()) {
            command_queue.push([& c, msg] {
                LOGD("", "MESSAGE ENQUEUED: size={}", msg.size());
                c.enqueue(msg);
            });
        } else {
            LOGE("", "Requester disconnected");
        }
    });

    LOGD("", "Requester ready");
    ready_cv.notify_one();

    while (!s_finish_flag) {
        c.step(std::chrono::milliseconds{10});
        command_queue.call_all();
    }
}

int main (int argc, char * argv[])
{
    //netty::socket4_addr listener_saddr {netty::inet4_addr::any_addr_value, 42142};
    netty::socket4_addr listener_saddr {netty::inet4_addr{127, 0, 0, 1}, 42142};

    auto commandLine = pfs::make_argvapi(argc, argv);
    auto programName = commandLine.program_name();
    auto commandLineIterator = commandLine.begin();
    PollerEnum backend {PollerEnum::Unknown};

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
            } else if (x.is_option("backend")) {
                if (!x.has_arg()) {
                    print_usage(programName, "Expected backend");
                    return EXIT_FAILURE;
                }

                if (x.arg() == "") {
#if NETTY__SELECT_ENABLED
                } else if (x.arg() == "select" ) {
                    backend = PollerEnum::Select;
#endif

#if NETTY__POLL_ENABLED
                } else if (x.arg() == "poll" ) {
                    backend = PollerEnum::Poll;
#endif

#if NETTY__EPOLL_ENABLED
                } else if (x.arg() == "epoll" ) {
                    backend = PollerEnum::EPoll;
#endif

#if NETTY__UDT_ENABLED
                } else if (x.arg() == "udt" ) {
                    backend = PollerEnum::UDT;
#endif

#if NETTY__ENET_ENABLED
                } else if (x.arg() == "enet" ) {
                    backend = PollerEnum::ENet;
#endif
                } else {
                    print_usage(programName, "Bad backend");
                    return EXIT_FAILURE;
                }
            } else {
                fmt::println(stderr, "Bad arguments. Try --help option.");
                return EXIT_FAILURE;
            }
        }
    }

    if (backend == PollerEnum::Unknown) {
        fmt::println(stderr, "Backend must be specified. Try --help option.");
        return EXIT_FAILURE;
    }

    std::mutex ready_mtx;
    std::condition_variable ready_cv;
    std::unique_lock<std::mutex> ready_lk(ready_mtx);

    std::thread service_thread {[backend, listener_saddr, & ready_cv] () {
#if NETTY__SELECT_ENABLED
        if (backend == PollerEnum::Select) {
            service_process<PollerEnum::Select>(listener_saddr, ready_cv);
            return;
        }
#endif

#if NETTY__POLL_ENABLED
        if (backend == PollerEnum::Poll) {
            service_process<PollerEnum::Poll>(listener_saddr, ready_cv);
            return;
        }
#endif

#if NETTY__EPOLL_ENABLED
        if (backend == PollerEnum::EPoll) {
            service_process<PollerEnum::EPoll>(listener_saddr, ready_cv);
            return;
        }
#endif

#if NETTY__UDT_ENABLED
        if (backend == PollerEnum::UDT) {
            service_process<PollerEnum::UDT>(listener_saddr, ready_cv);
            return;
        }
#endif

#if NETTY__ENET_ENABLED
        if (backend == PollerEnum::ENet) {
            service_process<PollerEnum::ENet>(listener_saddr, ready_cv);
            return;
        }
#endif
    }};

    ready_cv.wait(ready_lk);
    client_commands cmd;

    std::thread client_thread {[backend, listener_saddr, & ready_cv, & cmd] () {
#if NETTY__SELECT_ENABLED
        if (backend == PollerEnum::Select) {
            client_process<PollerEnum::Select>(cmd, listener_saddr, ready_cv);
            return;
        }
#endif

#if NETTY__POLL_ENABLED
        if (backend == PollerEnum::Poll) {
            client_process<PollerEnum::Poll>(cmd, listener_saddr, ready_cv);
            return;
        }
#endif

#if NETTY__EPOLL_ENABLED
        if (backend == PollerEnum::EPoll) {
            client_process<PollerEnum::EPoll>(cmd, listener_saddr, ready_cv);
            return;
        }
#endif

#if NETTY__UDT_ENABLED
        if (backend == PollerEnum::UDT) {
            client_process<PollerEnum::UDT>(cmd, listener_saddr, ready_cv);
            return;
        }
#endif

#if NETTY__ENET_ENABLED
        if (backend == PollerEnum::ENet) {
            client_process<PollerEnum::ENet>(cmd, listener_saddr, ready_cv);
            return;
        }
#endif
    }};

    ready_cv.wait(ready_lk);

    LOGD("", "Service and client threads ready");

    linenoiseSetCompletionCallback(completion);
    linenoiseSetHintsCallback(hints);

    while (true) {
        auto line = linenoise("client> ");

        if (line == nullptr) {
            s_finish_flag = true;
            break;
        }

        string_view sv {line};

        if (sv == "/e" || sv == "/q" || sv == "/exit" || sv == "/quit") {
            s_finish_flag = true;
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
            output_envelope_t env {message_enum::echo, ms.take()};
            cmd.send(env.take());

            if (s_connected_flag)
                ready_cv.wait(ready_lk);
        }
    }

    service_thread.join();
    client_thread.join();

    return EXIT_SUCCESS;
}
