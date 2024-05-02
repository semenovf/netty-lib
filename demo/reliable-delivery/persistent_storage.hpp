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
#include "netty/p2p/simple_message_id.hpp"
#include "pfs/debby/relational_database.hpp"
#include "pfs/debby/result.hpp"
#include "pfs/debby/statement.hpp"
#include "pfs/debby/backend/sqlite3/database.hpp"
#include "pfs/debby/backend/sqlite3/result.hpp"
#include <functional>
#include <memory>
#include <unordered_map>

class persistent_storage
{
public:
    using message_id_traits = netty::p2p::simple_message_id_traits;
    using message_id        = message_id_traits::type;

private:
    using relational_database = debby::relational_database<debby::backend::sqlite3::database>;
    using result_type         = debby::result<debby::backend::sqlite3::result>;
    using statement_type      = debby::statement<debby::backend::sqlite3::statement>;

    struct peer_info
    {
        message_id msgid;
    };

private:
    // Database for storing messages awaiting delivery confirmation.
    std::unique_ptr<relational_database> _delivery_dbh;

    // Peers cache
    std::unordered_map<netty::p2p::peer_id, peer_info> _peers;

public:
    persistent_storage (pfs::filesystem::path const & database_folder);

public:
    /**
     * Save message data into persistent storage.
     *
     * @param addressee Message addressee.
     * @param data Message serialized data.
     * @param len Length of the message serialized data.
     * @param perr Pointer to store an error if it occurs. May be @c nullptr.
     *
     * @note This method is `netty::p2p::reliable_delivery_engine` requirements.
     */
    message_id save (netty::p2p::peer_id addressee, char const * data, int len, pfs::error * perr = nullptr);

    /**
     * Remove message data from persistent storage.
     *
     * @param addressee Message addressee.
     * @param msgid Unique message identifier.
     * @param perr Pointer to store an error if it occurs. May be @c nullptr.
     *
     * @note This method is `netty::p2p::reliable_delivery_engine` requirements.
     */
    void remove (netty::p2p::peer_id addressee, message_id msgid, pfs::error * perr = nullptr);

private:
    void for_each (netty::p2p::peer_id peer_id, std::function<void (message_id, std::vector<char>)> f);
    void create_delivary_table (netty::p2p::peer_id peer_id);
    message_id fetch_recent_msgid (netty::p2p::peer_id peer_id);
};


