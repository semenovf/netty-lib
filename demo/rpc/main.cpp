////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.01 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "pfs/fmt.hpp"
#include "pfs/log.hpp"
#include "pfs/binary_istream.hpp"
#include "pfs/binary_ostream.hpp"
#include "pfs/crc16.hpp"
#include "pfs/string_view.hpp"
#include "pfs/netty/client_socket_engine.hpp"
#include "pfs/netty/default_poller_types.hpp"
#include "pfs/netty/socket4_addr.hpp"
#include "pfs/netty/startup.hpp"
#include "pfs/netty/posix/tcp_socket.hpp"
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

static char const * TAG = "NETTY";

static struct program_context {
    std::string program;
} __pctx;

static void print_usage ()
{
    fmt::print(stdout, "Usage\n\t{} --server-saddr=ip4_addr:port\n", __pctx.program);
}

struct packet
{
    std::string json;
};


template <typename packet>
void exec_command (packet const & pkt)
{
    // Here must be execution of the command
}

// Protocol envelope
//
//   1     2             3             4    5
// --------------------------------------------
// |x10|  size | p a y l o a d ... |crc16|x11|
// --------------------------------------------
// Field 1 - BEGIN flag (1 bytes).
// Field 2 - Payload size (4 bytes).
// Field 3 - Payload (n bytes).
// Field 4 - CRC16 of the payload (2 bytes).
// Field 5 - END flag (1 byte).
//
class protocol
{
    using size_field_type = std::uint32_t;
    using crc16_field_type = std::int16_t;

private:
    static constexpr const char BEGIN = '\xBF';
    static constexpr const char END   = '\xEF';
    static constexpr const int MIN_PACKET_SIZE = sizeof(BEGIN)
        + sizeof(size_field_type) + sizeof(crc16_field_type) + sizeof(END);

public:
    std::vector<char> serialize (packet const & p)
    {
        std::vector<char> result;
        std::uint32_t sz = sizeof(size_field_type) + p.json.size()
            + sizeof(crc16_field_type) + sizeof(END);

        pfs::binary_ostream<> out {result};
        //                ---- payload size and payload here
        //                |
        //                v
        out << BEGIN << p.json << pfs::crc16_of(p.json) << END;

        return result;
    }

    bool has_complete_packet (char const * data, std::size_t len)
    {
        if (len < MIN_PACKET_SIZE)
            return false;

        char b;
        std::uint32_t sz;
        pfs::binary_istream<> in {data, data + len};
        in >> b >> sz;

        return sz + sizeof(BEGIN) <= len;
    }

    std::pair<bool, std::size_t> exec (char const * data, std::size_t len)
    {
        if (len == 0)
            return std::make_pair(true, 0);

        pfs::binary_istream<> in {data, data + len};
        char b, e;
        packet pkt;
        crc16_field_type crc16;

        in >> b >> pkt.json >> crc16 >> e;

        bool success = (b == BEGIN && e == END);

        if (success)
            success = (crc16 == pfs::crc16_of(pkt.json));

        if (!success)
            return std::make_pair(false, 0);

        exec_command(pkt);

        return std::make_pair(true, static_cast<std::size_t>(in.begin() - data));
    }
};

using client_socket_engine = netty::client_socket_engine_mt<netty::posix::tcp_socket
    , netty::client_poller_type, protocol>;

int main (int argc, char * argv[])
{
    netty::startup_guard netty_startup;

    netty::socket4_addr server_saddr;

    __pctx.program = argv[0];

    if (argc == 1) {
        print_usage();
        return EXIT_SUCCESS;
    }

    for (int i = 1; i < argc; i++) {
        if (string_view{"-h"} == argv[i] || string_view{"--help"} == argv[i]) {
            print_usage();
            return EXIT_SUCCESS;
        } else if (starts_with(string_view{argv[i]}, "--server-saddr=")) {
            auto res = netty::socket4_addr::parse(argv[i] + 15);

            if (!res.first) {
                LOGE(TAG, "Bad server address");
                return EXIT_FAILURE;
            }

            server_saddr = res.second;
        } else {
            auto arglen = std::strlen(argv[i]);

            if (arglen > 0 && argv[i][0] == '-') {
                LOGE(TAG, "Bad option: {}", argv[i]);
                return EXIT_FAILURE;
            }
        }
    }

    client_socket_engine::options opts = client_socket_engine::default_options();
    client_socket_engine client_socket {opts};

    client_socket.connected = [& client_socket] () {
        LOGD(TAG, "CONNECTED");

        packet pkt1 {
            R"_({"jsonrpc": "2.0", "method": "hello", "params": {"greeting": "Hello, World!"}})_"
        };

        client_socket.send(pkt1);
    };

    client_socket.disconnected = [] {
        LOGD(TAG, "DISCONNECTED");
    };

    if (client_socket.connect(server_saddr)) {
        client_socket_engine::loop_result rs = client_socket_engine::loop_result::success;

        while (rs == client_socket_engine::loop_result::success) {
            rs = client_socket.loop();
        }

        if (rs != client_socket_engine::loop_result::success) {
            LOGD(TAG, "Client socket stopped by reason: {}"
                , rs == client_socket_engine::loop_result::disconnected
                    ? "DISCONNECTED"
                    : rs == client_socket_engine::loop_result::connection_refused
                        ? "CONNECTION REFUSED"
                        : "UNKNOWN");
        }
    }

    return EXIT_SUCCESS;
}
