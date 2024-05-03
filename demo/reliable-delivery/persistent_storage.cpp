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
    _delivery_dbh = relational_database::make_unique(delivery_db_path, true);
}

persistent_storage::envelope_id
persistent_storage::save (netty::p2p::peer_id addressee, char const * data, int len, pfs::error * perr)
{
    static char const * INSERT_DATA = "INSERT INTO `#{}` (eid, payload) VALUES (:eid, :payload)";

    envelope_id eid = envelope_traits::initial();

    try {
        auto pos = _peers.find(addressee);

        if (pos == _peers.end()) {
            create_delivary_table(addressee);
            eid = fetch_recent_eid(addressee);
            LOGD("~~~", "FETCHED ENVELOPE ID: {}", eid);
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
            stmt.exec();
            return true;
        });
    } catch (debby::error const & ex) {
        pfs::throw_or(perr, pfs::error {ex.code(), ex.what()});
        return envelope_traits::initial();
    }

    return eid;
}

void persistent_storage::remove (netty::p2p::peer_id addressee, envelope_id eid, pfs::error * perr)
{
    static char const * DELETE_DATA = "DELETE FROM `#{}` WHERE eid = :eid";

    debby::error err;
    auto sql = fmt::format(DELETE_DATA, to_string(addressee));

    _delivery_dbh->transaction([this, & sql, & eid] () {
        auto stmt = _delivery_dbh->prepare(sql, false);

        stmt.bind(":eid", eid);
        stmt.exec();

        return true;
    }, & err);

    if (err) {
        pfs::throw_or(perr, pfs::error {err.code(), err.what()});
    }
}

void persistent_storage::create_delivary_table (netty::p2p::peer_id peer_id)
{
    static char const * CREATE_DELIVERY_TABLE =
        "CREATE TABLE IF NOT EXISTS `#{}` ("
        "eid {} UNIQUE NOT NULL PRIMARY KEY" // Уникальный идентификатор конверта
        ", payload BLOB NOT NULL)"
        " WITHOUT ROWID";

    _delivery_dbh->transaction([this, & peer_id] () {
        auto sql = fmt::format(CREATE_DELIVERY_TABLE
            , to_string(peer_id)
            , affinity_traits<envelope_id>::name());
        _delivery_dbh->query(sql);
        return true;
    });
}

void persistent_storage::for_each (netty::p2p::peer_id peer_id
    , std::function<void (envelope_id, std::vector<char>)> f)
{
    static char const * SELECT_ALL_MESSAGES_ASC = "SELECT eid, payload FROM `#{}` ORDER BY eid ASC";
    auto sql = fmt::format(SELECT_ALL_MESSAGES_ASC, to_string(peer_id));

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
