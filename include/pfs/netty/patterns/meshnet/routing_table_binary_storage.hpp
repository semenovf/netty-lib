////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.03.09 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../error.hpp"
#include "../../namespace.hpp"
#include <pfs/filesystem.hpp>
#include <pfs/binary_istream.hpp>
#include <pfs/binary_ostream.hpp>
#include <pfs/i18n.hpp>
#include <pfs/ionik/local_file.hpp>
#include <cstdint>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

/**
 * Storage implementation for routing table
 */
template <typename NodeIdTraits>
class routing_table_binary_storage
{
public:
    using node_id = typename NodeIdTraits::node_id;

private:
    struct load_context
    {
        pfs::binary_istream<> * in {nullptr};
        std::uint16_t gwcount {0};
        std::uint16_t routecount {0};
    };

    struct save_context
    {
        pfs::binary_ostream<> out;
        std::uint16_t gwcount {0};
        std::uint16_t routecount {0};
    };

private:
    pfs::filesystem::path _path;
    load_context _load_ctx;
    save_context _save_ctx;

public:
    routing_table_binary_storage (pfs::filesystem::path const & path)
        : _path(path)
    {}

public:
    ////////////////////////////////////////////////////////////////////////////////////////////////
    // load session
    ////////////////////////////////////////////////////////////////////////////////////////////////
    template <typename F>
    void load_session (F && f)
    {
        if (!pfs::filesystem::exists(_path))
            return;

        ionik::error err;
        auto data = ionik::local_file::read_all(_path, & err);

        if (err) {
            throw error {tr::f_("load routing table from file failure: {}", err.what())};
        }

        if (data.empty())
            return;

        pfs::binary_istream<> in {data.data(), data.size()};
        _load_ctx.in = & in;

        _load_ctx.in->start_transaction();
        *_load_ctx.in >> _load_ctx.gwcount >> _load_ctx.routecount;

        f();

        if (!_load_ctx.in->commit_transaction()) {
            throw error {tr::f_("load routing table from file failure: {}: bad data", _path)};
        }
    }

    /**
     * @param @a f signature is @c void(node_id gwid).
     */
    template <typename F>
    void foreach_gateway (F && f)
    {
        while (_load_ctx.gwcount > 0) {
            node_id gwid;
            *_load_ctx.in >> gwid;

            if (_load_ctx.in->is_good())
                f(gwid);

            _load_ctx.gwcount--;
        }
    }

    /**
     * @param @a f signature is void(node_id id, node_id gwid, unsigned int hops)
     */
    template <typename F>
    void foreach_route (F && f)
    {
        while (_load_ctx.routecount > 0) {
            node_id id;
            node_id gwid;
            std::uint16_t hops;

            *_load_ctx.in >> id >> gwid >> hops;

            if (_load_ctx.in->is_good())
                f(id, gwid, static_cast<unsigned int>(hops));

            _load_ctx.routecount--;
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // save session
    ////////////////////////////////////////////////////////////////////////////////////////////////
    template <typename F>
    void save_session (F && f)
    {
        _save_ctx.out.reset();
        _save_ctx.gwcount = 0;
        _save_ctx.routecount = 0;

        // Reserve space for gwcount and routecount
        _save_ctx.out << _save_ctx.gwcount << _save_ctx.routecount;

        f();

        pfs::binary_ostream<> out;
        out << _save_ctx.gwcount << _save_ctx.routecount;

        auto data0 = out.take();
        auto data1 = _save_ctx.out.take();

        std::copy(data0.data(), data0.data() + data0.size(), data1.data());

        auto err = ionik::local_file::rewrite(_path, data1.data(), data1.size());

        if (err) {
            throw error {tr::f_("save routing table to file failure: {}: {}", _path, err->what())};
        }

        _save_ctx.out.reset();
    }

    void store_gateway (node_id gwid)
    {
        _save_ctx.out << gwid;
        _save_ctx.gwcount++;
    }

    void store_route (node_id dest, node_id gwid, unsigned int hops)
    {
        _save_ctx.out << dest << gwid << static_cast<std::uint16_t>(hops);
        _save_ctx.routecount++;
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
