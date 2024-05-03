////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.05.02 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "persistent_storage.hpp"

#include "pfs/log.hpp"

using namespace debby::backend::sqlite3;

persistent_storage::persistent_storage (pfs::filesystem::path const & database_folder)
{
    auto delivery_db_path = database_folder / PFS__LITERAL_PATH("delivery.db");
    auto ack_db_path = database_folder / PFS__LITERAL_PATH("delivery_ack.db");

    _delivery_dbh = relational_database::make_unique(delivery_db_path, true);
    _ack_dbh = keyvalue_database::make_unique(ack_db_path, true);
}

void persistent_storage::create_delivary_table (netty::p2p::peer_id peer_id)
{
    static char const * CREATE_DELIVERY_TABLE =
        "CREATE TABLE IF NOT EXISTS `#{}` ("
        "eid {} UNIQUE NOT NULL PRIMARY KEY" // Уникальный идентификатор конверта
        ", payload BLOB NOT NULL"
        ", ack {} NOT NULL)"
        " WITHOUT ROWID";

    _delivery_dbh->transaction([this, & peer_id] () {
        auto sql = fmt::format(CREATE_DELIVERY_TABLE
            , to_string(peer_id)
            , affinity_traits<envelope_id>::name()
            , affinity_traits<bool>::name());
        _delivery_dbh->query(sql);
        return true;
    });
}

persistent_storage::envelope_id
persistent_storage::save (netty::p2p::peer_id addressee, char const * data, int len)
{
    static char const * INSERT_DATA = "INSERT INTO `#{}` (eid, payload, ack) VALUES (:eid, :payload, :ack)";

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
    _peers[addressee] = peer_info {eid};

    // Persist message data
    _delivery_dbh->transaction([this, & addressee, eid, data, len] () {
        auto sql = fmt::format(INSERT_DATA, to_string(addressee));
        auto stmt = _delivery_dbh->prepare(sql, false);

        stmt.bind(":eid", eid);
        stmt.bind(":payload", data, len, debby::transient_enum::no);
        stmt.bind(":ack", false);
        stmt.exec();
        return true;
    });

    return eid;
}

void persistent_storage::ack (netty::p2p::peer_id addressee, envelope_id eid)
{
    static char const * ACK_ENVELOPE = "UPDATE OR IGNORE `#{}` SET ack=:ack WHERE eid = :eid";

    auto sql = fmt::format(ACK_ENVELOPE, to_string(addressee));

    _delivery_dbh->transaction([this, & sql, & eid] () {
        auto stmt = _delivery_dbh->prepare(sql, false);

        stmt.bind(":ack", true);
        stmt.bind(":eid", eid);
        stmt.exec();

        return true;
    });
}

void persistent_storage::nack (netty::p2p::peer_id addressee, envelope_id eid)
{
    ack(addressee, eid);
}

void persistent_storage::set_recent_eid (netty::p2p::peer_id addresser, envelope_id eid)
{
    _ack_dbh->set(to_string(addresser), eid);
}

persistent_storage::envelope_id persistent_storage::recent_eid (netty::p2p::peer_id addresser)
{
    return _ack_dbh->get_or<envelope_id>(to_string(addresser), envelope_traits::initial());
}

void persistent_storage::maintain (netty::p2p::peer_id peer_id)
{
    static char const * DELETE_DATA = "DELETE FROM `#{}` WHERE ack=TRUE";

    auto sql = fmt::format(DELETE_DATA, to_string(peer_id));

    _delivery_dbh->transaction([this, & sql] () {
        auto stmt = _delivery_dbh->prepare(sql, false);
        stmt.exec();
        return true;
    });
}

void persistent_storage::for_each (netty::p2p::peer_id peer_id
    , std::function<void (envelope_id, std::vector<char>)> f)
{
    static char const * SELECT_ALL_ENVELOPES_ASC = "SELECT eid, payload FROM `#{}` ORDER BY eid ASC";

    auto sql = fmt::format(SELECT_ALL_ENVELOPES_ASC, to_string(peer_id));

    _delivery_dbh->transaction([this, & sql, & f] () {
        auto stmt = _delivery_dbh->prepare(sql);
        auto res = stmt.exec();

        while (res.has_more()) {
            envelope_id eid;
            std::vector<char> payload;

            res["eid"] >> eid;
            res["payload"] >> payload;

            f(eid, std::move(payload));

            res.next();
        }

        return true;
    });
}

void persistent_storage::for_each_eid_greater (envelope_id eid, netty::p2p::peer_id peer_id
    , std::function<void (envelope_id, std::vector<char>)> f)
{
    static char const * SELECT_GREATER_ENVELOPES_ASC = "SELECT eid, payload FROM `#{}` WHERE eid > :eid ORDER BY eid ASC";
    auto sql = fmt::format(SELECT_GREATER_ENVELOPES_ASC, to_string(peer_id));

    _delivery_dbh->transaction([this, & sql, & eid, & f] () {
        auto stmt = _delivery_dbh->prepare(sql);
        stmt.bind("eid", eid);
        auto res = stmt.exec();

        while (res.has_more()) {
            envelope_id eid;
            std::vector<char> payload;

            res["eid"] >> eid;
            res["payload"] >> payload;

            f(eid, std::move(payload));

            res.next();
        }

        return true;
    });
}

persistent_storage::envelope_id
persistent_storage::fetch_recent_eid (netty::p2p::peer_id peer_id)
{
    static char const * SELECT_RECENT_MESSAGE = "SELECT eid FROM `#{}` ORDER BY eid DESC";

    auto eid = envelope_traits::initial();
    auto sql = fmt::format(SELECT_RECENT_MESSAGE, to_string(peer_id));

    _delivery_dbh->transaction([this, & sql, & eid] () {
        auto stmt = _delivery_dbh->prepare(sql);
        auto res = stmt.exec();

        if (res.has_more())
            res["eid"] >> eid;

        return true;
    });

    return eid;
}
