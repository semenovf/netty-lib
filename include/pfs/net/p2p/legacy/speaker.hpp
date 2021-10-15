////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.09.29 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "endpoint.hpp"
#include "envelope.hpp"
#include "pfs/emitter.hpp"
#include "pfs/uuid.hpp"
#include <chrono>
#include <map>
#include <string>

namespace pfs {
namespace net {
namespace p2p {

template <typename Impl, typename EndpointImpl>
class basic_speaker
{
    using endpoint_impl_type = EndpointImpl;

public: // signals
    pfs::emitter_mt<std::shared_ptr<endpoint_impl_type>> connected;
    pfs::emitter_mt<std::shared_ptr<endpoint_impl_type>> disconnected;
    pfs::emitter_mt<std::shared_ptr<endpoint_impl_type>, std::string const &> endpoint_failure;

protected:
    basic_speaker () = default;
    ~basic_speaker () = default;

    basic_speaker (basic_speaker const &) = delete;
    basic_speaker & operator = (basic_speaker const &) = delete;

    basic_speaker (basic_speaker &&) = default;
    basic_speaker & operator = (basic_speaker &&) = default;

public:
    void connect (uuid_t peer_uuid, inet4_addr const & addr, std::uint16_t port)
    {
        static_cast<Impl *>(this)->connect_impl(peer_uuid, addr, port);
    }
};

}}} // namespace pfs::net::p2p

