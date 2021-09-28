////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.09.27 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "backend_enum.hpp"
#include "connection.hpp"
#include "pfs/emitter.hpp"
#include "pfs/net/exports.hpp"
// #include "pfs/net/inet4_addr.hpp"
// #include <functional>
// #include <memory>
// #include <string>

namespace pfs {
namespace net {
namespace p2p {

template <backend_enum Backend>
class PFS_NET_LIB_DLL_EXPORT connection_manager final
{
    using connection_type = connection<Backend>;

public:
    connection_manager ()
    {

    }

    ~connection_manager () = default;

    connection_manager (connection_manager const &) = delete;
    connection_manager & operator = (connection_manager const &) = delete;

    connection_manager (connection_manager &&) = default;
    connection_manager & operator = (connection_manager &&) = default;
};

}}} // namespace pfs::net::p2p
