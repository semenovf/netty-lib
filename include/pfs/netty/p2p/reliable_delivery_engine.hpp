////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.04.23 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "peer_id.hpp"
#include "pfs/i18n.hpp"
#include "pfs/optional.hpp"
#include <memory>

namespace netty {
namespace p2p {

template <typename DeliveryEngine, typename PersistentStorage>
class reliable_delivery_engine
{
    using serializer_type = typename DeliveryEngine::serializer_type;

private:
    std::unique_ptr<DeliveryEngine> _delivery;
    std::unique_ptr<PersistentStorage> _storage;

public:
    /**
     * Initializes underlying APIs and constructs delivery engine instance.
     *
     * @param host_id Unique host identifier for this instance.
     */
    reliable_delivery_engine (std::unique_ptr<DeliveryEngine> && delivery_engine
        , std::unique_ptr<PersistentStorage> && storage
        , error * perr = nullptr)
        : _delivery(std::move(delivery_engine))
        , _storage(std::move(storage))
    {
        if (!_delivery) {
            pfs::throw_or(perr, error {
                  make_error_code(std::errc::invalid_argument)
                , tr::_("delivery engine is null")
            });

            return;
        }

        if (!_storage) {
            pfs::throw_or(perr, error {
                  make_error_code(std::errc::invalid_argument)
                , tr::_("persistent storage is null")
            });

            return;
        }
    }

    ~reliable_delivery_engine () = default;

    reliable_delivery_engine (reliable_delivery_engine const &) = delete;
    reliable_delivery_engine & operator = (reliable_delivery_engine const &) = delete;

    reliable_delivery_engine (reliable_delivery_engine &&) = default;
    reliable_delivery_engine & operator = (reliable_delivery_engine &&) = default;

public:
    int step (std::chrono::milliseconds timeout = std::chrono::milliseconds{0})
    {
        return _delivery->step(timeout);
    }

    void connect (host4_addr haddr)
    {
        _delivery->connect(std::move(haddr));
    }

    void release_peer (peer_id peerid)
    {
        _delivery->release_peer(peerid);
    }

    bool enqueue (peer_id addressee, char const * data, int len)
    {
        pfs::error err;
        auto msgid = _storage->save(addressee, data, len, & err);

        if (err) {
            _delivery->on_failure(netty::error{err.code(), tr::f_("save message failure: {}"
                , err.what())});
            return false;
        }

        typename serializer_type::ostream_type out;
        out << msgid << std::make_pair(data, len);

        auto success = _delivery->enqueue(addressee, out.data());

        if (!success) {
            _storage->remove(addressee, msgid, & err);

            if (err) {
                _delivery->on_failure(netty::error{err.code(), tr::f_("remove message failure: {}: {}"
                    , msgid, err.what())});
                return false;
            }

            return false;
        }

        return true;
    }

    bool enqueue (peer_id addressee, std::string const & data)
    {
        return enqueue(addressee, data.data(), data.size());
    }

    bool enqueue (peer_id addressee, std::vector<char> const & data)
    {
        return enqueue(addressee, data.data(), data.size());
    }

    void file_upload_stopped (universal_id addressee, universal_id fileid)
    {
        _delivery->file_upload_stopped(addressee, fileid);
    }

    void file_upload_complete (universal_id addressee, universal_id fileid)
    {
        _delivery->file_upload_complete(addressee, fileid);
    }

    void file_ready_send (universal_id addressee, universal_id fileid, packet_type_enum packettype
        , typename serializer_type::output_archive_type data)
    {
        _delivery->file_ready_send(addressee, fileid, packettype, std::move(data));
    }

private:
};

}} // namespace netty::p2p


