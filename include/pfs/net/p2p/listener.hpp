////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.09.20 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/net/exports.hpp"
#include "connection.hpp"
#include "backend_enum.hpp"
#include "pfs/emitter.hpp"
#include <chrono>
#include <memory>
#include <string>
#include <utility>

namespace pfs {
namespace net {
namespace p2p {

template <backend_enum Backend>
class PFS_NET_LIB_DLL_EXPORT listener final
{
public:
    struct options
    {
        // Address to bind listener ('*' is any address)
        std::string listener_addr4 {"*"};
        std::uint16_t listener_port {42424};
        std::string listener_interface {"*"};
    };

private:
    class backend;
    std::unique_ptr<backend> _p;

    using connection_type = connection<Backend>;

public: // signals
    pfs::emitter_mt<std::string const & /*error*/> failure;
    pfs::emitter_mt<connection_type const &> accepted;

public:
    listener ();
    ~listener ();

    listener (listener const &) = delete;
    listener & operator = (listener const &) = delete;

    listener (listener &&);
    listener & operator = (listener &&);

    bool set_options (options && opts);

    bool start ();
    void stop ();

    bool started () const noexcept;
};

}}} // namespace pfs::net::p2p
