////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.04.23 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "functional_reliable_delivery_callbacks.hpp"
#include "peer_id.hpp"
#include "simple_message_id.hpp"
#include "pfs/i18n.hpp"
#include <cstdint>
#include <memory>

namespace netty {
namespace p2p {

enum class message_type_enum : std::uint8_t
{
      data = 0
        /// Message itself.

    , ack = 1
        /// Message receive acknowledgement (used by reliable_delivery_engine).
        /// Packet content depends on message identifier.

    , nack = 2
        /// Message used to notify sender the message already process early (used by
        /// reliable_delivery_engine). Packet content depends on message identifier.
};

template <typename MessageIdTraits = simple_message_id_traits>
struct message_header
{
    message_type_enum type;
    typename MessageIdTraits::type id;
};

template <typename DeliveryEngine, typename PersistentStorage
    , typename Callbacks = functional_reliable_delivery_callbacks>
class reliable_delivery_engine: public DeliveryEngine, public Callbacks
{
public:
    using serializer_type = typename DeliveryEngine::serializer_type;

private:
    std::unique_ptr<PersistentStorage> _storage;

public:
    /**
     * Initializes underlying APIs and constructs delivery engine instance.
     *
     * @param host_id Unique host identifier for this instance.
     */
    template <typename ...Args>
    reliable_delivery_engine (std::unique_ptr<PersistentStorage> && storage
        , Args &&... delivery_engine_args)
        : DeliveryEngine(std::forward<Args>(delivery_engine_args)...)
        , _storage(std::move(storage))
    {
        if (!_storage) {
            throw error {
                  make_error_code(std::errc::invalid_argument)
                , tr::_("persistent storage is null")
            };

            return;
        }

        DeliveryEngine::data_received = [this] (universal_id addresser, std::vector<char> data) {
            typename serializer_type::istream_type in{data.data(), data.size()};
            netty::p2p::message_type_enum msgtype;
            persistent_storage::message_id msgid;
            std::vector<char> payload;
            in >> msgtype >> msgid >> payload;

            Callbacks::message_received(addresser, payload);
        };
    }

    ~reliable_delivery_engine () = default;

    reliable_delivery_engine (reliable_delivery_engine const &) = delete;
    reliable_delivery_engine & operator = (reliable_delivery_engine const &) = delete;

    reliable_delivery_engine (reliable_delivery_engine &&) = default;
    reliable_delivery_engine & operator = (reliable_delivery_engine &&) = default;

public:
    bool enqueue (peer_id addressee, char const * data, int len)
    {
        pfs::error err;
        auto msgid = _storage->save(addressee, data, len, & err);

        if (err) {
            this->on_failure(netty::error{err.code(), tr::f_("save message failure: {}", err.what())});
            return false;
        }

        typename serializer_type::ostream_type out;
        out << message_type_enum::data << msgid << std::make_pair(data, len);

        auto success = DeliveryEngine::enqueue(addressee, out.take());

        if (!success) {
            _storage->remove(addressee, msgid, & err);

            if (err) {
                this->on_failure(netty::error{err.code(), tr::f_("remove message failure: {}: {}"
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

private:
};

}} // namespace netty::p2p
