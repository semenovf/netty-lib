////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.05.02 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "persistent_storage.hpp"
#include <pfs/debby/data_definition.hpp>
#include <pfs/netty/error.hpp>

namespace netty {
namespace sample {

namespace fs = pfs::filesystem;
using data_definition_t = debby::data_definition<debby::backend_enum::sqlite3>;

persistent_storage::persistent_storage (fs::path const & database_folder
    , std::string const & delivery_db_name
    , std::string const & delivery_ack_db_name)
{
    auto delivery_db_path = database_folder / fs::utf8_decode(delivery_db_name);
    _ack_db_path = database_folder / fs::utf8_decode(delivery_ack_db_name);

    _delivery_db = relational_database::make(delivery_db_path, true);
    _ack_db = keyvalue_database::make(_ack_db_path, true);
}

persistent_storage::~persistent_storage ()
{
    if (_wipe_on_destroy)
        wipe();
}

inline std::string delivery_table_name (netty::p2p::peer_id peer_id)
{
    return '#' + to_string(peer_id);
}

void persistent_storage::create_delivary_table (netty::p2p::peer_id peer_id)
{
    auto delivery_table = data_definition_t::create_table(delivery_table_name(peer_id));
    delivery_table.add_column<envelope_id>("eid").primary_key().unique();
    delivery_table.add_column<debby::blob_t>("payload");
    delivery_table.add_column<bool>("ack");
    delivery_table.constraint("WITHOUT ROWID");

    auto recent_id_table = data_definition_t::create_table("eids");
    recent_id_table.add_column<decltype(peer_id)>("peer_id").primary_key().unique();
    recent_id_table.add_column<envelope_id>("eid");

    auto sqls = {delivery_table.build(), recent_id_table.build()};

    auto failure = _delivery_db.transaction([& sqls, db = & _delivery_db] () {
        for (auto const & sql: sqls)
            db->query(sql);

        return pfs::optional<std::string>{};
    });
}

void persistent_storage::meet_peer (netty::p2p::peer_id peerid)
{
    create_delivary_table(peerid);
    auto eid = fetch_recent_eid(peerid);
    _peers[peerid] = peer_info {eid};
}

void persistent_storage::spend_peer (netty::p2p::peer_id peerid)
{
    _peers.erase(peerid);
}

persistent_storage::envelope_id
persistent_storage::save (netty::p2p::peer_id addressee, char const * data, int len)
{
    static char const * INSERT_DATA = "INSERT INTO \"{}\" (eid, payload, ack) VALUES (:eid, :payload, :ack)";
    static char const * REPLACE_EID_DATA = "REPLACE INTO `eids` (peer_id, eid) VALUES (:peer_id, :eid)";

    envelope_id eid = envelope_traits::initial();

    auto pos = _peers.find(addressee);

    if (pos == _peers.end()) {
        create_delivary_table(addressee);
        eid = fetch_recent_eid(addressee);
    } else {
        eid = pos->second.eid;
    }

    // Reserve new message ID
    eid = envelope_traits::next(eid);

    // Persist message data
    _delivery_db.transaction([this, & addressee, eid, data, len] () {
        auto sql = fmt::format(INSERT_DATA, delivery_table_name(addressee));
        auto stmt = _delivery_db.prepare_cached(sql);

        stmt.bind(":eid", eid);
        stmt.bind(":payload", data, len);
        stmt.bind(":ack", false);
        stmt.exec();

        sql = std::string(REPLACE_EID_DATA);
        auto stmt1 = _delivery_db.prepare_cached(sql);

        stmt1.bind(":peer_id", addressee);
        stmt1.bind(":eid", eid);
        stmt1.exec();

        return pfs::optional<std::string>{};
    });

    _peers[addressee] = peer_info {eid};

    return eid;
}

void persistent_storage::ack (netty::p2p::peer_id addressee, envelope_id eid)
{
    static char const * ACK_ENVELOPE = "UPDATE OR IGNORE \"{}\" SET ack=:ack WHERE eid = :eid";

    auto sql = fmt::format(ACK_ENVELOPE, delivery_table_name(addressee));
    auto stmt = _delivery_db.prepare_cached(sql);

    stmt.bind(":ack", true);
    stmt.bind(":eid", eid);
    stmt.exec();
}

void persistent_storage::nack (netty::p2p::peer_id addressee, envelope_id eid)
{
    ack(addressee, eid);
}

void persistent_storage::again (envelope_id eid, netty::p2p::peer_id addressee
    , std::function<void (envelope_id, std::string)> f)
{
    for_each_eid_greater(eid, addressee, std::move(f));
}

void persistent_storage::again (netty::p2p::peer_id addressee, std::function<void (envelope_id, std::string)> f)
{
    for_each_unacked(addressee, std::move(f));
}

void persistent_storage::set_recent_eid (netty::p2p::peer_id addresser, envelope_id eid)
{
    _ack_db.set(to_string(addresser), eid);
}

persistent_storage::envelope_id persistent_storage::recent_eid (netty::p2p::peer_id addresser)
{
    return _ack_db.get_or<envelope_id>(to_string(addresser), envelope_traits::initial());
}

void persistent_storage::maintain (netty::p2p::peer_id peer_id)
{
    static char const * DELETE_DATA = "DELETE FROM \"{}\" WHERE ack=TRUE";

    auto table_exists = _delivery_db.exists(delivery_table_name(peer_id));

    if (!table_exists)
        return;

    auto sql = fmt::format(DELETE_DATA, delivery_table_name(peer_id));
    _delivery_db.query(sql);
}

void persistent_storage::wipe ()
{
    auto tables = _delivery_db.tables("^#");

    if (!tables.empty())
        _delivery_db.remove(tables);

    _ack_db.wipe(_ack_db_path);
}

void persistent_storage::for_each (netty::p2p::peer_id peer_id, std::function<void (envelope_id, std::string)> f)
{
    static char const * SELECT_ALL_ENVELOPES_ASC = "SELECT eid, payload FROM \"{}\" ORDER BY eid ASC";

    auto sql = fmt::format(SELECT_ALL_ENVELOPES_ASC, delivery_table_name(peer_id));

    _delivery_db.transaction([this, & sql, & f] () {
        auto res = _delivery_db.exec(sql);

        while (res.has_more()) {
            auto eid = res.get<envelope_id>("eid");
            auto payload = res.get<std::string>("payload");

            f(*eid, std::move(*payload));

            res.next();
        }

        return pfs::optional<std::string>{};
    });
}

void persistent_storage::for_each_eid_greater (envelope_id eid, netty::p2p::peer_id peer_id
    , std::function<void (envelope_id, std::string)> f)
{
    static char const * SELECT_GREATER_ENVELOPES_ASC = "SELECT eid, payload FROM \"{}\" WHERE eid > :eid ORDER BY eid ASC";
    auto sql = fmt::format(SELECT_GREATER_ENVELOPES_ASC, delivery_table_name(peer_id));

    auto stmt = _delivery_db.prepare_cached(sql);
    stmt.bind("eid", eid);
    auto res = stmt.exec();

    while (res.has_more()) {
        auto eid = res.get<envelope_id>("eid");
        auto payload = res.get<std::string>("payload");

        f(*eid, std::move(*payload));

        res.next();
    }
}

void persistent_storage::for_each_unacked (netty::p2p::peer_id peer_id
    , std::function<void (envelope_id, std::string)> f)
{
    static char const * SELECT_GREATER_ENVELOPES_ASC = "SELECT eid, payload FROM \"{}\" WHERE ack = FALSE ORDER BY eid ASC";
    auto sql = fmt::format(SELECT_GREATER_ENVELOPES_ASC, delivery_table_name(peer_id));
    auto res = _delivery_db.exec(sql);

    while (res.has_more()) {
        auto eid = res.get<envelope_id>("eid");
        auto payload = res.get<std::string>("payload");

        f(*eid, std::move(*payload));

        res.next();
    }
}

persistent_storage::envelope_id
persistent_storage::fetch_recent_eid (netty::p2p::peer_id peer_id)
{
    static char const * SELECT_RECENT_EID = "SELECT eid FROM eids WHERE peer_id = :peer_id";

    auto eid = envelope_traits::initial();
    auto sql = std::string(SELECT_RECENT_EID);
    auto stmt = _delivery_db.prepare_cached(sql);
    stmt.bind(":peer_id", peer_id);
    auto res = stmt.exec();

    if (!res.has_more()) {
        throw error {
              pfs::errc::unexpected_error
            , tr::f_("fetch recent envelope id failure for peer: {}", peer_id)
        };
    }

    eid = *res.get<envelope_id>("eid");
    return eid;
}

}} // namespace netty::sample
