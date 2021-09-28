////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.09.20 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "backend_enum.hpp"
#include "pfs/emitter.hpp"
#include "pfs/net/exports.hpp"
#include "pfs/net/inet4_addr.hpp"
#include <functional>
#include <memory>
#include <string>

namespace pfs {
namespace net {
namespace p2p {

template <backend_enum Backend>
class listener;

template <backend_enum Backend>
class PFS_NET_LIB_DLL_EXPORT connection final
{
    friend class listener<Backend>;

    class backend;
    std::unique_ptr<backend> _p;

public: // signals
    emitter_mt<connection &> connected;
    emitter_mt<connection &> disconnected;
    pfs::emitter_mt<connection &, std::string const & /*error*/> failure;

public:
    connection ();
    ~connection ();

    connection (connection const &) = delete;
    connection & operator = (connection const &) = delete;

    connection (connection &&);
    connection & operator = (connection &&);

    /**
     * Connects to host asynchronously (non-blocking).
     */
    void connect (inet4_addr const & addr, std::uint16_t port);
};

}}} // namespace pfs::net::p2p
