////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.02.13 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "protocol.hpp"
#include "serial_id.hpp"
#include "../../error.hpp"
#include "../../namespace.hpp"
#include <pfs/assert.hpp>
#include <pfs/i18n.hpp>
#include <pfs/utility.hpp>
#include <chrono>
#include <functional>
#include <vector>

// FIXME REMOVE
#include <pfs/log.hpp>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace reliable_delivery {

/*
 * Callbacks API:
 *--------------------------------------------------------------------------------------------------
 * class callabcks
 * {
 * public:
 *      void on_payload (std::vector<char> && payload); // Payload handler
 *      void on_report (std::vector<char> && payload);  // Report handler
 *      void dispatch (std::vector<char> && msg);       // Dispatch message handler
 * }
 */
template <typename IncomeProcessor
    , typename OutcomeProcessor
    , typename SerializerTraits
    , typename CallbackSuite>
class manager
{
    using income_processor_type = IncomeProcessor;
    using outcome_processor_type = OutcomeProcessor;
    using serializer_traits = SerializerTraits;

public:
    using serializer_type = typename serializer_traits::serializer_type;
    using callback_suite = CallbackSuite;

private:
    income_processor_type * _inproc {nullptr};
    outcome_processor_type * _outproc {nullptr};
    callback_suite _callbacks;

    // FIXME REMOVE (for debug purposes)
    std::string _name;

public:
    manager (std::string name
        , income_processor_type & income_proc
        , outcome_processor_type & outcome_proc
        , callback_suite && callbacks)
        : _inproc(& income_proc)
        , _outproc(& outcome_proc)
        , _callbacks(std::move(callbacks))
        , _name(std::move(name))
    {}

    manager (manager const &) = delete;
    manager (manager &&) = delete;
    manager & operator = (manager const &) = delete;
    manager & operator = (manager &&) = delete;

    ~manager () {}

public:
    /**
     * Pack `payload` and save it in storage before sending.
     */
    std::vector<char> payload (char const * data, std::size_t length)
    {
        auto out = serializer_traits::make_serializer();
        auto sid = _outproc->next_serial();
        payload_packet pkt{sid};
        pkt.serialize(out, data, length);
        auto msg = out.take();
        _outproc->cache(sid, msg.data(), msg.size());

        LOGD(_name.c_str(), "SND: PAYLOAD: sid={}", sid);

        return msg;
    }

    /**
     * Pack `report` before sending.
     */
    std::vector<char> report (char const * data, std::size_t length)
    {
        auto out = serializer_traits::make_serializer();
        report_packet pkt{};
        pkt.serialize(out, data, length);

        LOGD(_name.c_str(), "SND: REPORT");

        return out.take();
    }

    /**
     * Pack `ack` before sending.
     */
    std::vector<char> ack (serial_id sid)
    {
        auto out = serializer_traits::make_serializer();
        ack_packet pkt{sid};
        pkt.serialize(out);

        LOGD(_name.c_str(), "SND: ACK: sid={}", sid);

        return out.take();
    }

    std::vector<char> nack (serial_id sid)
    {
        auto out = serializer_traits::make_serializer();
        nack_packet pkt{sid};
        pkt.serialize(out);

        LOGD(_name.c_str(), "SND: NACK: sid={}", sid);

        return out.take();
    }

    std::vector<char> again (serial_id sid)
    {
        auto out = serializer_traits::make_serializer();
        again_packet pkt{sid};
        pkt.serialize(out);

        LOGD(_name.c_str(), "SND: AGAIN: sid={}", sid);

        return out.take();
    }

    std::vector<char> again (std::vector<serial_id> const & missed)
    {
        auto out = serializer_traits::make_serializer();

        for (auto const & sid: missed) {
            again_packet pkt{sid};

            LOGD(_name.c_str(), "SND: GROUP AGAIN: sid={}", sid);

            pkt.serialize(out);
        }

        return out.take();
    }

    void process_packet (std::vector<char> && data)
    {
        PFS__ASSERT(data.size() > 0, "");

        auto in = serializer_traits::make_deserializer(data.data(), data.size());
        in.start_transaction();

        // Data can contains more than one packet (see again() method for group of packets)
        do {
            header h {in};

            if (in.is_good()) {
                switch (h.type()) {
                    case packet_enum::payload: {
                        payload_packet pkt {h, in};

                        if (!in.commit_transaction())
                            break;

                        if (_inproc->payload_expected(h.id())) {
                            LOGD(_name.c_str(), "RCV: PAYLOAD: ACK: sid={}", h.id());

                            // Send prepared `ack` packet
                            _callbacks.dispatch(ack(h.id()));

                            // Process payload
                            _callbacks.on_payload(std::move(pkt.bytes));

                            // Update (increment to next value) committed serial ID
                            _inproc->commit(h.id());
                        } else if (_inproc->payload_duplicated(h.id())) {
                            LOGD(_name.c_str(), "RCV: PAYLOAD: NACK: sid={}", h.id());

                            _callbacks.dispatch(nack(h.id()));
                        } else {
                            LOGD(_name.c_str(), "RCV: PAYLOAD: AGAIN: sid={}", h.id());

                            // Previous payloads are lost.
                            // Cache current payload.
                            _inproc->cache(h.id(), std::move(pkt.bytes));

                            // Obtain list of missed serial IDs
                            auto missed = _inproc->missed(h.id());
                            _callbacks.dispatch(again(missed));
                        }

                        break;
                    }

                    case packet_enum::report: {
                        LOGD(_name.c_str(), "RCV: REPORT");

                        report_packet pkt {h, in};

                        if (in.commit_transaction())
                            _callbacks.on_report(std::move(pkt.bytes));

                        break;
                    }

                    case packet_enum::ack: {
                        LOGD(_name.c_str(), "RCV: ACK: sid={}", h.id());

                        ack_packet pkt {h, in};

                        if (in.commit_transaction())
                            _outproc->ack(h.id());

                        break;
                    }

                    case packet_enum::nack: {
                        LOGD(_name.c_str(), "RCV: NACK: sid={}", h.id());
                        nack_packet pkt {h, in};

                        // The processing is the same as for `ack`
                        if (in.commit_transaction())
                            _outproc->ack(h.id());

                        break;
                    }

                    case packet_enum::again: {
                        LOGD(_name.c_str(), "RCV: AGAIN: sid={}", h.id());

                        ack_packet pkt {h, in};

                        if (in.commit_transaction()) {
                            auto msg = _outproc->payload(h.id());
                            _callbacks.dispatch(std::move(msg));
                        }
                        break;
                    }

                    default:
                        throw error {
                              pfs::errc::unexpected_data
                            , tr::f_("unexpected packet type: {}", pfs::to_underlying(h.type()))
                        };

                        break;
                }
            }

            if (!in.is_good()) {
                throw error {
                      pfs::errc::unexpected_data
                    , tr::f_("bad or corrupted header for reliable delivery packet")
                };
            }
        } while (in.available() > 0);
    }

    bool has_waiting () const
    {
        return _outproc->has_waiting();
    }

    void step ()
    {
        _outproc->foreach_waiting([this] (std::vector<char> data) {
            _callbacks.dispatch(std::move(data));
        });
    }
};

}} // namespace patterns::reliable_delivery

NETTY__NAMESPACE_END
