////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.05.02 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "persistent_storage.hpp"

namespace netty {
namespace sample {

using namespace debby::backend::sqlite3;
namespace fs = pfs::filesystem;

persistent_storage::persistent_storage (fs::path const & database_folder
    , std::string const & delivery_db_name
    , std::string const & delivery_ack_db_name)
{
    auto delivery_db_path = database_folder / fs::utf8_decode(delivery_db_name);
    _ack_db_path = database_folder / fs::utf8_decode(delivery_ack_db_name);

    _delivery_dbh = relational_database::make_unique(delivery_db_path, true);
    _ack_dbh = keyvalue_database::make_unique(_ack_db_path, true);
}

persistent_storage::~persistent_storage ()
{
    if (_wipe_on_destroy)
        wipe();
}

void persistent_storage::create_delivary_table (netty::p2p::peer_id peer_id)
{
    static char const * CREATE_DELIVERY_TABLE =
        "CREATE TABLE IF NOT EXISTS `#{}` ("
        "eid {} UNIQUE NOT NULL PRIMARY KEY" // Уникальный идентификатор конверта
        ", payload BLOB NOT NULL"
        ", ack {} NOT NULL)"
        " WITHOUT ROWID";

    static char const * CREATE_RECENT_EID_TABLE =
        "CREATE TABLE IF NOT EXISTS `eids` ("
        "peer_id {} UNIQUE NOT NULL PRIMARY KEY"
        ", eid {} NOT NULL)" // Уникальный идентификатор конверта
        " WITHOUT ROWID";

    _delivery_dbh->transaction([this, & peer_id] () {
        auto sql = fmt::format(CREATE_DELIVERY_TABLE
            , to_string(peer_id)
            , affinity_traits<envelope_id>::name()
            , affinity_traits<bool>::name());

        _delivery_dbh->query(sql);

        sql = fmt::format(CREATE_RECENT_EID_TABLE
            , affinity_traits<decltype(peer_id)>::name()
            , affinity_traits<envelope_id>::name());

        _delivery_dbh->query(sql);

        return true;
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
    static char const * INSERT_DATA = "INSERT INTO `#{}` (eid, payload, ack) VALUES (:eid, :payload, :ack)";
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
    _delivery_dbh->transaction([this, & addressee, eid, data, len] () {
        auto sql = fmt::format(INSERT_DATA, to_string(addressee));
        auto stmt = _delivery_dbh->prepare(sql, true);

        stmt.bind(":eid", eid);
        stmt.bind(":payload", data, len, debby::transient_enum::no);
        stmt.bind(":ack", false);
        stmt.exec();

        sql = std::string(REPLACE_EID_DATA);
        auto stmt1 = _delivery_dbh->prepare(sql, true);

        stmt1.bind(":peer_id", addressee);
        stmt1.bind(":eid", eid);
        stmt1.exec();

        return true;
    });

    _peers[addressee] = peer_info {eid};

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

void persistent_storage::again (envelope_id eid, netty::p2p::peer_id addressee
    , std::function<void (envelope_id, std::vector<char>)> f)
{
    for_each_eid_greater(eid, addressee, std::move(f));
}

void persistent_storage::again (netty::p2p::peer_id addressee
    , std::function<void (envelope_id, std::vector<char>)> f)
{
    for_each_unacked(addressee, std::move(f));
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
    auto table_exists = _delivery_dbh->exists(fmt::format("#{}", to_string(peer_id)));

    if (!table_exists)
        return;

    static char const * DELETE_DATA = "DELETE FROM `#{}` WHERE ack=TRUE";

    auto sql = fmt::format(DELETE_DATA, to_string(peer_id));

    _delivery_dbh->transaction([this, & sql] () {
        auto stmt = _delivery_dbh->prepare(sql, false);
        stmt.exec();
        return true;
    });
}

void persistent_storage::wipe ()
{
    auto tables = _delivery_dbh->tables("^#");

    if (!tables.empty())
        _delivery_dbh->remove(tables);

    _ack_dbh->wipe(_ack_db_path);
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

void persistent_storage::for_each_unacked (netty::p2p::peer_id peer_id
    , std::function<void (envelope_id, std::vector<char>)> f)
{
    static char const * SELECT_GREATER_ENVELOPES_ASC = "SELECT eid, payload FROM `#{}` WHERE ack = FALSE ORDER BY eid ASC";
    auto sql = fmt::format(SELECT_GREATER_ENVELOPES_ASC, to_string(peer_id));

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

persistent_storage::envelope_id
persistent_storage::fetch_recent_eid (netty::p2p::peer_id peer_id)
{
    static char const * SELECT_RECENT_EID = "SELECT eid FROM `eids` WHERE peer_id = :peer_id";

    auto eid = envelope_traits::initial();
    auto sql = std::string(SELECT_RECENT_EID);

    _delivery_dbh->transaction([this, & sql, peer_id, & eid] () {
        auto stmt = _delivery_dbh->prepare(sql);
        stmt.bind(":peer_id", peer_id);
        auto res = stmt.exec();

        if (res.has_more())
            res["eid"] >> eid;
        // else
        //  use initial value

        return true;
    });

    return eid;
}

}} // namespace netty::sample
