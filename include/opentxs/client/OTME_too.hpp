/************************************************************
 *
 *                 OPEN TRANSACTIONS
 *
 *       Financial Cryptography and Digital Cash
 *       Library, Protocol, API, Server, CLI, GUI
 *
 *       -- Anonymous Numbered Accounts.
 *       -- Untraceable Digital Cash.
 *       -- Triple-Signed Receipts.
 *       -- Cheques, Vouchers, Transfers, Inboxes.
 *       -- Basket Currencies, Markets, Payment Plans.
 *       -- Signed, XML, Ricardian-style Contracts.
 *       -- Scripted smart contracts.
 *
 *  EMAIL:
 *  fellowtraveler@opentransactions.org
 *
 *  WEBSITE:
 *  http://www.opentransactions.org/
 *
 *  -----------------------------------------------------
 *
 *   LICENSE:
 *   This Source Code Form is subject to the terms of the
 *   Mozilla Public License, v. 2.0. If a copy of the MPL
 *   was not distributed with this file, You can obtain one
 *   at http://mozilla.org/MPL/2.0/.
 *
 *   DISCLAIMER:
 *   This program is distributed in the hope that it will
 *   be useful, but WITHOUT ANY WARRANTY; without even the
 *   implied warranty of MERCHANTABILITY or FITNESS FOR A
 *   PARTICULAR PURPOSE.  See the Mozilla Public License
 *   for more details.
 *
 ************************************************************/

#ifndef OPENTXS_CLIENT_OTME_TOO_HPP
#define OPENTXS_CLIENT_OTME_TOO_HPP

#include "opentxs/core/Proto.hpp"

#include <atomic>
#include <cstdint>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <tuple>

namespace opentxs
{

class Api;
class MadeEasy;
class Nym;
class OT_ME;
class OTAPI_Exec;
class Settings;

class OTME_too
{
private:
    friend class Api;

    typedef std::map<proto::ContactItemType, std::string> unitTypeMap;
    typedef std::map<std::string, proto::ContactItemType> typeUnitMap;
    typedef std::tuple<
        std::uint64_t,  // index
        std::string,    // owner nym
        std::string,    // admin password
        std::string,    // notary ID
        unitTypeMap,    // unit IDs
        unitTypeMap,    // account IDs
        bool,           // backup started
        bool,           // connected
        bool            // done
        > PairedNode;
    typedef std::map<std::string, PairedNode> PairedNodes;
    typedef std::map<
        std::string,
        std::tuple<
            std::string,
            std::string,
            std::string>> ServerNameData;

    std::recursive_mutex& api_lock_;
    Settings& config_;
    OTAPI_Exec& exec_;
    const MadeEasy& made_easy_;
    const OT_ME& otme_;

    mutable std::atomic<bool> pairing_;
    mutable std::atomic<bool> refreshing_;
    mutable std::atomic<std::uint64_t> refresh_count_;
    mutable std::mutex pair_initiate_lock_;
    mutable std::mutex pair_lock_;
    mutable std::unique_ptr<std::thread> pairing_thread_;
    mutable std::unique_ptr<std::thread> refresh_thread_;

    PairedNodes paired_nodes_;

    bool check_accounts(PairedNode& node);
    bool check_backup(const std::string& bridgeNymID, PairedNode& node);
    bool check_bridge_nym(
        const std::string& bridgeNym,
        PairedNode& node);
    bool check_introduction_server(const std::string& withNym) const;
    void check_server_names();
    bool check_server_registration(
        const std::string& nym,
        const std::string& server) const;
    bool download_nym(
        const std::string& localNym,
        const std::string& remoteNym,
        const std::string& server = "") const;
    std::uint64_t extract_assets(
        const proto::ContactData& claims,
        PairedNode& node);
    std::string extract_server(const proto::ContactData& claims) const;
    std::string extract_server_name(const std::string& serverNymID) const;
    void fill_existing_accounts(
        const std::string& nym,
        std::uint64_t& have,
        typeUnitMap& neededUnits,
        unitTypeMap& neededAccounts,
        PairedNode& node);
    std::unique_ptr<PairedNode> find_node(const std::string& identifier) const;
    std::string get_introduction_server() const;
    bool insert_at_index(
        const std::int64_t index,
        const std::int64_t total,
        const std::string& myNym,
        const std::string& bridgeNym,
        const std::string& password) const;
    void mark_connected(PairedNode& node);
    void mark_finished(const std::string& bridgeNymID);
    std::string obtain_account(
        const std::string& nym,
        const std::string& id,
        const std::string& server) const;
    bool obtain_asset_contract(
        const std::string& nym,
        const std::string& id,
        const std::string& server) const;
    bool obtain_assets(
        const std::string& bridgeNym,
    const proto::ContactData& claims,
        PairedNode& node);
    std::unique_ptr<proto::ContactData> obtain_contact_data(
        const std::string& localNym,
        const Nym& remoteNym,
        const std::string& server) const;
    std::shared_ptr<const Nym> obtain_nym(
        const std::string& localNym,
        const std::string& remoteNym,
        const std::string& server) const;
    bool obtain_server_contract(
        const std::string& nym,
        const std::string& server) const;
    std::string obtain_server_id(
        const std::string& ownerNym,
        const std::string& bridgeNym,
        const proto::ContactData& claims) const;
    void pair(const std::string& bridgeNymID);
    std::uint64_t paired_nodes() const;
    void pairing_thread();
    void parse_pairing_section(std::uint64_t index);
    void refresh_thread();
    bool send_backup(const std::string& bridgeNymID, PairedNode& node) const;
    void send_server_name(
        const std::string& nym,
        const std::string& server,
        const std::string& password,
        const std::string& name) const;
    void set_server_names(const ServerNameData& servers);
    std::int64_t scan_incomplete_pairing(const std::string& bridgeNym);
    void scan_pairing();
    void Shutdown() const;
    bool update_accounts(const PairedNode& node);
    bool update_assets(PairedNode& node);
    bool update_notary(const std::string& id, PairedNode& node);
    proto::ContactItemType validate_unit(const std::int64_t type);

    OTME_too(
        std::recursive_mutex& lock,
        Settings& config,
        OTAPI_Exec& exec,
        const MadeEasy& madeEasy,
        const OT_ME& otme);
    OTME_too() = delete;
    OTME_too(const OTME_too&) = delete;
    OTME_too(const OTME_too&&) = delete;
    OTME_too& operator=(const OTME_too&) = delete;
    OTME_too& operator=(const OTME_too&&) = delete;

public:
    std::string GetPairedServer(const std::string& bridgeNymID) const;
    bool PairingComplete(const std::string& identifier) const;
    bool PairingStarted(const std::string& identifier) const;
    bool PairingSuccessful(const std::string& identifier) const;
    bool PairNode(
        const std::string& myNym,
        const std::string& bridgeNym,
        const std::string& password);
    void Refresh(const std::string& wallet = "");
    std::uint64_t RefreshCount() const;
    std::string SetIntroductionServer(const std::string& contract) const;
    void UpdatePairing(const std::string& wallet = "");

    ~OTME_too();
};
} // namespace opentxs

#endif // OPENTXS_CLIENT_OTME_TOO_HPP
