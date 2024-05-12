////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.05.01 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/error.hpp"
#include <pfs/filesystem.hpp>
#include "netty/p2p/peer_id.hpp"
#include "netty/p2p/simple_envelope.hpp"
#include "pfs/debby/keyvalue_database.hpp"
#include "pfs/debby/relational_database.hpp"
#include "pfs/debby/result.hpp"
#include "pfs/debby/statement.hpp"
#include "pfs/debby/backend/lmdb/database.hpp"
#include "pfs/debby/backend/sqlite3/database.hpp"
#include "pfs/debby/backend/sqlite3/result.hpp"
#include <functional>
#include <memory>
#include <unordered_map>

namespace netty {
namespace sample {

class persistent_storage
{
public:
    using envelope_traits = netty::p2p::simple_envelope_traits;
    using envelope_id     = envelope_traits::id;

private:
    using keyvalue_database   = debby::keyvalue_database<debby::backend::lmdb::database>;
    using relational_database = debby::relational_database<debby::backend::sqlite3::database>;
    using result_type         = debby::result<debby::backend::sqlite3::result>;
    using statement_type      = debby::statement<debby::backend::sqlite3::statement>;

    struct peer_info
    {
        envelope_id eid;
    };

private:
    bool _wipe_on_destroy {false};

    pfs::filesystem::path _ack_db_path;

    // Database for storing envelopes awaiting delivery confirmation.
    std::unique_ptr<relational_database> _delivery_dbh;

    // Database for storing envelope recent identifiers by receiver.
    std::unique_ptr<keyvalue_database> _ack_dbh;

    // Peers cache
    std::unordered_map<netty::p2p::peer_id, peer_info> _peers;

public:
    persistent_storage (pfs::filesystem::path const & database_folder
        , std::string const & delivery_db_name = std::string("delivery.db")
        , std::string const & delivery_ack_db_name = std::string("delivery_ack.db"));

    ~persistent_storage ();

public:
    void meet_peer (netty::p2p::peer_id peerid);
    void spend_peer (netty::p2p::peer_id peerid);

    /**
     * Save message data into persistent storage. Used by addresser.
     *
     * @param addressee Envelope addressee.
     * @param payload Serialized envelope payload.
     * @param len Length of the serialized envelope payload.
     *
     * @note This method is `netty::p2p::reliable_delivery_engine` requirements.
     */
    envelope_id save (netty::p2p::peer_id addressee, char const * payload, int len);

    /**
     * Commit the envelope in case of success delivery. Used by addresser.
     *
     * @param addressee Envelope addressee.
     * @param eid Unique envelope identifier.
     *
     * @note This method is `netty::p2p::reliable_delivery_engine` requirements.
     */
    void ack (netty::p2p::peer_id addressee, envelope_id eid);

    /**
     * Commit the envelope in case of expired (duplicated) delivery. Used by addresser.
     *
     * @param addressee Envelope addressee.
     * @param eid Unique envelope identifier.
     *
     * @note This method is `netty::p2p::reliable_delivery_engine` requirements.
     */
    void nack (netty::p2p::peer_id addressee, envelope_id eid);

    /**
     * Fetch envelopes to retransmit again to the peer @a addressee. Used by addresser.
     *
     * @param addressee Envelope addressee.
     * @param eid The unique identifier of the envelope from which the retransmission should occur.
     * @param f Callback to process fetched envelopes.
     *
     * @note This method is `netty::p2p::reliable_delivery_engine` requirements.
     */
    void again (envelope_id eid, netty::p2p::peer_id addressee
        , std::function<void (envelope_id, std::vector<char>)> f);

    /**
     * Fetch envelopes that is not ack'ed to retransmit again to the peer @a addressee.
     * Used by addresser.
     *
     * @param addressee Envelope addressee.
     * @param f Callback to process fetched envelopes.
     *
     * @note This method is `netty::p2p::reliable_delivery_engine` requirements.
     */
    void again (netty::p2p::peer_id addressee, std::function<void (envelope_id, std::vector<char>)> f);

    /**
     * Sets recent envelope ID associated with @a addresser. Used by addressee.
     *
     * @param addresser Envelope addresser.
     * @param eid Recent envelope ID.
     *
     * @note This method is `netty::p2p::reliable_delivery_engine` requirements.
     */
    void set_recent_eid (netty::p2p::peer_id addresser, envelope_id eid);

    /**
     * Fetch recent envelope ID associated with @a addresser. Used by addressee.
     *
     * @param addresser Envelope addresser.
     * @return Recent envelope ID.
     *
     * @note This method is `netty::p2p::reliable_delivery_engine` requirements.
     */
    envelope_id recent_eid (netty::p2p::peer_id addresser);

    /**
     * Maintain the storage (removed ack'ed records, etc)
     */
    void maintain (netty::p2p::peer_id peer_id);

    void wipe_on_destroy (bool enable)
    {
        _wipe_on_destroy = enable;
    }

private:
    void for_each (netty::p2p::peer_id peer_id, std::function<void (envelope_id, std::vector<char>)> f);
    void for_each_eid_greater (envelope_id eid, netty::p2p::peer_id peer_id, std::function<void (envelope_id, std::vector<char>)> f);
    void for_each_unacked (netty::p2p::peer_id peer_id, std::function<void (envelope_id, std::vector<char>)> f);
    void create_delivary_table (netty::p2p::peer_id peer_id);
    envelope_id fetch_recent_eid (netty::p2p::peer_id peer_id);
    void wipe ();
};

}} // namespace netty::sample
