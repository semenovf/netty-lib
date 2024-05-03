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
#include "simple_envelope.hpp"
#include "pfs/i18n.hpp"
#include <cstdint>
#include <memory>

namespace netty {
namespace p2p {

enum class envelope_type_enum : std::uint8_t
{
      payload = 0
        /// Envelope payload.

    , ack = 1
        /// Envelope receive acknowledgement (used by reliable_delivery_engine).

    , nack = 2
        /// An envelope used to notify the sender that the payload has already been processed
        /// previously. (used by reliable_delivery_engine).
};

template <typename EnvelopeTraits = simple_envelope_traits>
struct envelope_header
{
    envelope_type_enum etype;
    typename EnvelopeTraits::id eid;
};

template <typename DeliveryEngine, typename PersistentStorage
    , typename Callbacks = functional_reliable_delivery_callbacks>
class reliable_delivery_engine: public DeliveryEngine, public Callbacks
{
public:
    using serializer_type = typename DeliveryEngine::serializer_type;
    using envelope_traits = typename PersistentStorage::envelope_traits;
    using envelope_header_type = envelope_header<envelope_traits>;

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
            envelope_header_type h;
            // netty::p2p::envelope_type_enum etype;
            // persistent_storage::envelope_id eid;
            std::vector<char> payload;
            in >> h.etype >> h.eid >> payload;

            Callbacks::envelope_received(addresser, payload);
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
        auto eid = _storage->save(addressee, data, len, & err);

        if (err) {
            this->on_failure(netty::error{err.code(), tr::f_("save envelope failure: {}", err.what())});
            return false;
        }

        typename serializer_type::ostream_type out;
        envelope_header_type h {envelope_type_enum::payload, eid};

        out << h.etype << h.eid << std::make_pair(data, len);

        return DeliveryEngine::enqueue(addressee, out.take());
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
