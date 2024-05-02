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

persistent_storage::message_id
persistent_storage::save (netty::p2p::peer_id addressee, char const * data, int len, pfs::error * perr)
{
    static char const * INSERT_DATA = "INSERT INTO `#{}` (msgid, data) VALUES (:msgid, :data)";

    message_id msgid = message_id_traits::initial();

    try {
        auto pos = _peers.find(addressee);

        if (pos == _peers.end()) {
            create_delivary_table(addressee);
            msgid = fetch_recent_msgid(addressee);
            LOGD("~~~", "FETCHED MSGID: {}", msgid);
        } else {
            msgid = pos->second.msgid;
        }

        // Reserve new message ID
        msgid = message_id_traits::next(msgid);
        _peers[addressee] = peer_info {msgid};

        // Persist message data
        _delivery_dbh->transaction([this, & addressee, msgid, data, len] () {
            auto sql = fmt::format(INSERT_DATA, to_string(addressee));
            auto stmt = _delivery_dbh->prepare(sql, false);

            stmt.bind(":msgid", msgid);
            stmt.bind(":data", data, len, debby::transient_enum::no);
            stmt.exec();
            return true;
        });
    } catch (debby::error const & ex) {
        pfs::throw_or(perr, pfs::error {ex.code(), ex.what()});
        return message_id_traits::initial();
    }

    return msgid;
}

void persistent_storage::remove (netty::p2p::peer_id addressee, message_id msgid, pfs::error * perr)
{
    static char const * DELETE_DATA = "DELETE FROM `#{}` WHERE msgid = :msgid";

    debby::error err;
    auto sql = fmt::format(DELETE_DATA, to_string(addressee));

    _delivery_dbh->transaction([this, & sql, & msgid] () {
        auto stmt = _delivery_dbh->prepare(sql, false);

        stmt.bind(":msgid", msgid);
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
        "msgid {} UNIQUE NOT NULL PRIMARY KEY" // Уникальный идентификатор сообщения
        ", data BLOB NOT NULL)"
        " WITHOUT ROWID";

    _delivery_dbh->transaction([this, & peer_id] () {
        auto sql = fmt::format(CREATE_DELIVERY_TABLE
            , to_string(peer_id)
            , affinity_traits<message_id>::name());
        _delivery_dbh->query(sql);
        return true;
    });
}

void persistent_storage::for_each (netty::p2p::peer_id peer_id
    , std::function<void (message_id, std::vector<char>)> f)
{
    static char const * SELECT_ALL_MESSAGES_ASC = "SELECT msgid, data FROM `#{}` ORDER BY msgid ASC";
    auto sql = fmt::format(SELECT_ALL_MESSAGES_ASC, to_string(peer_id));

    _delivery_dbh->transaction([this, & sql, & f] () {
        auto stmt = _delivery_dbh->prepare(sql);
        auto res = stmt.exec();

        while (res.has_more()) {
            message_id msgid;
            std::vector<char> payload;

            res["msgid"] >> msgid;
            res["data"] >> payload;

            f(msgid, std::move(payload));

            res.next();
        }

        return true;
    });
}

persistent_storage::message_id
persistent_storage::fetch_recent_msgid (netty::p2p::peer_id peer_id)
{
    static char const * SELECT_RECENT_MESSAGE = "SELECT msgid FROM `#{}` ORDER BY msgid DESC";

    auto msgid = message_id_traits::initial();
    auto sql = fmt::format(SELECT_RECENT_MESSAGE, to_string(peer_id));

    _delivery_dbh->transaction([this, & sql, & msgid] () {
        auto stmt = _delivery_dbh->prepare(sql);
        auto res = stmt.exec();

        if (res.has_more())
            res["msgid"] >> msgid;

        return true;
    });

    return msgid;
}
