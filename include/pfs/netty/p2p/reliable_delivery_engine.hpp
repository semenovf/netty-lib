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
#include "simple_envelope.hpp"
#include <pfs/i18n.hpp>
#include <pfs/utility.hpp>
#include <cstdint>
#include <memory>
#include <utility>

namespace netty {
namespace p2p {

enum class envelope_type_enum : std::uint8_t
{
      payload = 0
        /// Envelope payload.

    , ack = 1
        /// Envelope receive acknowledgement.

    , nack = 2
        /// An envelope used to notify the sender that the payload has already been processed
        /// previously.

    , again = 3
        /// Request the retransmission of envelopes.

    , report = 4
        /// Payload without need acknowledgement.
};

template <typename EnvelopeTraits = simple_envelope_traits>
struct envelope_header
{
    envelope_type_enum etype;
    typename EnvelopeTraits::id eid;
};

template <typename DeliveryEngine, typename PersistentStorage>
class reliable_delivery_engine: public DeliveryEngine
{
public:
    using serializer_type = typename DeliveryEngine::serializer_type;
    using envelope_traits = typename PersistentStorage::envelope_traits;
    using envelope_id     = typename PersistentStorage::envelope_id;
    using envelope_header_type = envelope_header<envelope_traits>;

private:
    std::unique_ptr<PersistentStorage> _storage;

    decltype(DeliveryEngine::data_received) _data_received_cb;
    decltype(DeliveryEngine::channel_established) _channel_established_cb;
    decltype(DeliveryEngine::channel_closed) _channel_closed_cb;

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
    }

    ~reliable_delivery_engine () = default;

    reliable_delivery_engine (reliable_delivery_engine const &) = delete;
    reliable_delivery_engine & operator = (reliable_delivery_engine const &) = delete;

    reliable_delivery_engine (reliable_delivery_engine &&) = default;
    reliable_delivery_engine & operator = (reliable_delivery_engine &&) = default;

public:
    /**
     * Call this method before main loop to complete engine configuration.
     */
    void ready ()
    {
        _data_received_cb = std::move(DeliveryEngine::data_received);
        _channel_established_cb = std::move(DeliveryEngine::channel_established);
        _channel_closed_cb = std::move(DeliveryEngine::channel_closed);

        DeliveryEngine::data_received = [this] (peer_id addresser, std::vector<char> data) {
            process_data_received(addresser, std::move(data));
        };

        DeliveryEngine::channel_established = [this] (netty::host4_addr haddr) {
            process_channel_established(std::move(haddr));
        };

        DeliveryEngine::channel_closed = [this] (peer_id peerid) {
            process_channel_closed(peerid);
        };
    }

    bool enqueue (peer_id addressee, char const * data, int len)
    {
        typename serializer_type::ostream_type out;

        try {
            auto eid = _storage->save(addressee, data, len);
            envelope_header_type h {envelope_type_enum::payload, eid};
            out << h.etype << h.eid << std::make_pair(data, len);

            LOG_TRACE_3("{} <- PAYLOAD: {:06}", addressee, eid);
        } catch (std::system_error const & ex ) {
            this->on_failure(netty::error{ex.code(), tr::f_("save envelope failure: {}", ex.what())});
            return false;
        } catch (...) {
            this->on_failure(netty::error{make_error_code(pfs::errc::unexpected_error)
                , tr::_("save envelope failure")});
            return false;
        }

        return DeliveryEngine::enqueue(addressee, out.take());
    }

    bool enqueue (peer_id addressee, std::string const & data)
    {
        return enqueue(addressee, data.data(), data.size());
    }

    bool enqueue (peer_id addressee, std::vector<char> data)
    {
        return enqueue(addressee, data.data(), data.size());
    }

    bool enqueue_report (peer_id addressee, char const * data, int len)
    {
        typename serializer_type::ostream_type out;
        envelope_header_type h {envelope_type_enum::report, 0};
        out << h.etype << h.eid << std::make_pair(data, len);

        LOG_TRACE_3("{} <- REPORT: {:06}", addressee, h.eid);
        return DeliveryEngine::enqueue(addressee, out.take());
    }

    bool enqueue_report (peer_id addressee, std::string const & data)
    {
        return enqueue_report(addressee, data.data(), data.size());
    }

    void wipe_on_destroy (bool enable)
    {
        _storage->wipe_on_destroy(enable);
    }

private:
    bool enqueue_ack (peer_id addressee, envelope_id eid)
    {
        typename serializer_type::ostream_type out;

        try {
            envelope_header_type h {envelope_type_enum::ack, eid};
            out << h.etype << h.eid;
        } catch (std::system_error const & ex ) {
            this->on_failure(netty::error{ex.code(), tr::f_("`ack` envelope failure: {}", ex.what())});
            return false;
        } catch (...) {
            this->on_failure(netty::error{make_error_code(pfs::errc::unexpected_error)
                , tr::_("`ack` envelope failure")});
            return false;
        }

        LOG_TRACE_3("{} <- ACK: {:06}", addressee, eid);
        return DeliveryEngine::enqueue(addressee, out.take());
    }

    bool enqueue_nack (peer_id addressee, envelope_id eid)
    {
        typename serializer_type::ostream_type out;

        try {
            envelope_header_type h {envelope_type_enum::nack, eid};
            out << h.etype << h.eid;
            LOG_TRACE_3("{} <- NACK: {:06}", addressee, eid);
        } catch (std::system_error const & ex ) {
            this->on_failure(netty::error{ex.code(), tr::f_("`nack` envelope failure: {}", ex.what())});
            return false;
        } catch (...) {
            this->on_failure(netty::error{make_error_code(pfs::errc::unexpected_error)
                , tr::_("`nack` envelope failure")});
            return false;
        }

        return DeliveryEngine::enqueue(addressee, out.take());
    }

    bool enqueue_again (peer_id addressee, envelope_id eid)
    {
        typename serializer_type::ostream_type out;

        try {
            envelope_header_type h {envelope_type_enum::nack, eid};
            out << h.etype << h.eid;
            LOG_TRACE_3("{} <- AGAIN: {:06}", addressee, eid);
        } catch (std::system_error const & ex ) {
            this->on_failure(netty::error{ex.code(), tr::f_("`again` envelope failure: {}", ex.what())});
            return false;
        } catch (...) {
            this->on_failure(netty::error{make_error_code(pfs::errc::unexpected_error)
                , tr::_("`again` envelope failure")});
            return false;
        }

        return DeliveryEngine::enqueue(addressee, out.take());
    }

    bool enqueue_payload_again (peer_id addressee, envelope_id eid, std::string const & payload)
    {
        typename serializer_type::ostream_type out;

        try {
            envelope_header_type h {envelope_type_enum::payload, eid};
            out << h.etype << h.eid << payload;
            LOG_TRACE_3("{} <- PAYLOAD AGAIN: {:06}", addressee, eid);
        } catch (std::system_error const & ex ) {
            this->on_failure(netty::error{ex.code(), tr::f_("retransmit `payload` envelope failure: {}: {}"
                , eid, ex.what())});
            return false;
        } catch (...) {
            this->on_failure(netty::error{make_error_code(pfs::errc::unexpected_error)
                , tr::f_("retransmit `payload` envelope failure: {}", eid)});
            return false;
        }

        return DeliveryEngine::enqueue(addressee, out.take());
    }

    std::pair<envelope_type_enum, envelope_id> check_eid_sequence (peer_id addresser, envelope_id eid)
    {
        auto recent_eid = _storage->recent_eid(addresser);

        if (envelope_traits::eq(eid, envelope_traits::next(recent_eid)))
            return std::make_pair(envelope_type_enum::ack, eid);

        if (envelope_traits::less_or_eq(eid, recent_eid))
            return std::make_pair(envelope_type_enum::nack, eid);

        return std::make_pair(envelope_type_enum::again, recent_eid);
    }

    void process_data_received (peer_id addresser, std::vector<char> data)
    {
        typename serializer_type::istream_type in{data.data(), data.size()};
        envelope_header_type h;
        std::vector<char> payload;
        in >> h.etype >> h.eid >> payload;

        switch (h.etype) {
            case envelope_type_enum::payload: {
                LOG_TRACE_3("{} -> PAYLOAD: {:06}", addresser, h.eid);

                auto res = check_eid_sequence(addresser, h.eid);

                switch (res.first) {
                    case envelope_type_enum::ack:
                        if (enqueue_ack(addresser, res.second)) {
                            this->_data_received_cb(addresser, payload);
                            _storage->set_recent_eid(addresser, res.second);
                        }
                        break;
                    case envelope_type_enum::nack:
                        enqueue_nack(addresser, res.second);
                        break;
                    case envelope_type_enum::again:
                        enqueue_again(addresser, res.second);
                        break;
                    default:
                        break;
                }

                break;
            }

            case envelope_type_enum::ack:
                LOG_TRACE_3("{} -> ACK: {:06}", addresser, h.eid);
                _storage->ack(addresser, h.eid);
                break;

            case envelope_type_enum::nack:
                LOG_TRACE_3("{} -> NACK: {:06}", addresser, h.eid);
                _storage->nack(addresser, h.eid);
                break;

            case envelope_type_enum::again:
                LOG_TRACE_3("{} -> AGAIN: {:06}", addresser, h.eid);
                _storage->again(h.eid, addresser, [this, addresser] (envelope_id eid, std::string payload) {
                    enqueue_payload_again(addresser, eid, std::move(payload));
                });
                break;

            case envelope_type_enum::report:
                LOG_TRACE_3("{} -> REPORT: {:06}", addresser, h.eid);
                this->_data_received_cb(addresser, payload);
                break;

            default:
                this->on_error(tr::f_("invalid envelope type: {}, ignored", pfs::to_underlying(h.etype)));
                break;
        }
    }

    void process_channel_established (netty::host4_addr haddr)
    {
        auto addressee = haddr.host_id;

        _storage->maintain(addressee);
        _storage->meet_peer(addressee);

        _storage->again(addressee, [this, addressee] (envelope_id eid, std::string payload) {
            enqueue_payload_again(addressee, eid, std::move(payload));
        });

        _channel_established_cb(std::move(haddr));
    }

    void process_channel_closed (peer_id peerid)
    {
        // Storage may be destroyed already on engine destruction before
        if (_storage) {
            _storage->maintain(peerid);
            _storage->spend_peer(peerid);
        }

        _channel_closed_cb(peerid);
    }
};

}} // namespace netty::p2p
