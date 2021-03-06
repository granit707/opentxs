// Copyright (c) 2010-2020 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "stdafx.hpp"

#include "opentxs/api/client/Issuer.hpp"
#include "opentxs/api/client/Manager.hpp"
#include "opentxs/api/network/Dht.hpp"
#include "opentxs/api/network/ZMQ.hpp"
#include "opentxs/api/server/Manager.hpp"
#include "opentxs/api/storage/Storage.hpp"
#include "opentxs/api/Core.hpp"
#include "opentxs/api/Endpoints.hpp"
#include "opentxs/api/Factory.hpp"
#if OT_CASH
#include "opentxs/blind/Purse.hpp"
#endif
#include "opentxs/client/NymData.hpp"
#include "opentxs/client/OT_API.hpp"
#include "opentxs/consensus/ClientContext.hpp"
#include "opentxs/consensus/Context.hpp"
#include "opentxs/consensus/ServerContext.hpp"
#include "opentxs/contact/Contact.hpp"
#include "opentxs/contact/ContactData.hpp"
#include "opentxs/core/contract/basket/BasketContract.hpp"
#include "opentxs/core/contract/peer/PeerObject.hpp"
#include "opentxs/core/contract/peer/PeerReply.hpp"
#include "opentxs/core/contract/peer/PeerRequest.hpp"
#include "opentxs/core/contract/CurrencyContract.hpp"
#include "opentxs/core/contract/SecurityContract.hpp"
#include "opentxs/core/contract/UnitDefinition.hpp"
#include "opentxs/core/identifier/Nym.hpp"
#include "opentxs/core/identifier/Server.hpp"
#include "opentxs/core/identifier/UnitDefinition.hpp"
#include "opentxs/core/Account.hpp"
#include "opentxs/core/Log.hpp"
#include "opentxs/core/Message.hpp"
#include "opentxs/core/OTTransactionType.hpp"
#include "opentxs/core/String.hpp"
#include "opentxs/identity/Nym.hpp"
#include "opentxs/network/zeromq/socket/Push.hpp"
#include "opentxs/network/zeromq/Context.hpp"
#include "opentxs/network/zeromq/Message.hpp"
#include "opentxs/Types.hpp"

#include "internal/api/client/Client.hpp"
#include "internal/core/Core.hpp"
#include "Exclusive.tpp"

#include <functional>
#include <stdexcept>

#include "Wallet.hpp"

template class opentxs::Exclusive<opentxs::Account>;
template class opentxs::Shared<opentxs::Account>;
template class opentxs::Pimpl<opentxs::network::zeromq::Message>;

#define OT_METHOD "opentxs::api::implementation::Wallet::"

namespace opentxs::api::implementation
{
const Wallet::UnitNameMap Wallet::unit_of_account_{
    {"BTC", proto::CITEMTYPE_BTC},   {"ETH", proto::CITEMTYPE_ETH},
    {"XRP", proto::CITEMTYPE_XRP},   {"LTC", proto::CITEMTYPE_LTC},
    {"DAO", proto::CITEMTYPE_DAO},   {"XEM", proto::CITEMTYPE_XEM},
    {"DAS", proto::CITEMTYPE_DASH},  {"LSK", proto::CITEMTYPE_LSK},
    {"DGD", proto::CITEMTYPE_DGD},   {"XMR", proto::CITEMTYPE_XMR},
    {"NXT", proto::CITEMTYPE_NXT},   {"AMP", proto::CITEMTYPE_AMP},
    {"XLM", proto::CITEMTYPE_XLM},   {"FCT", proto::CITEMTYPE_FCT},
    {"BTS", proto::CITEMTYPE_BTS},   {"USD", proto::CITEMTYPE_USD},
    {"EUR", proto::CITEMTYPE_EUR},   {"GBP", proto::CITEMTYPE_GBP},
    {"INR", proto::CITEMTYPE_INR},   {"AUD", proto::CITEMTYPE_AUD},
    {"CAD", proto::CITEMTYPE_CAD},   {"SGD", proto::CITEMTYPE_SGD},
    {"CHF", proto::CITEMTYPE_CHF},   {"MYR", proto::CITEMTYPE_MYR},
    {"JPY", proto::CITEMTYPE_JPY},   {"CNY", proto::CITEMTYPE_CNY},
    {"NZD", proto::CITEMTYPE_NZD},   {"THB", proto::CITEMTYPE_THB},
    {"HUF", proto::CITEMTYPE_HUF},   {"AED", proto::CITEMTYPE_AED},
    {"HKD", proto::CITEMTYPE_HKD},   {"MXN", proto::CITEMTYPE_MXN},
    {"ZAR", proto::CITEMTYPE_ZAR},   {"PHP", proto::CITEMTYPE_PHP},
    {"SEK", proto::CITEMTYPE_SEK},   {"BTT", proto::CITEMTYPE_TNBTC},
    {"LTT", proto::CITEMTYPE_TNLTC}, {"DAT", proto::CITEMTYPE_TNDASH},
    {"BCH", proto::CITEMTYPE_BCH},   {"BCT", proto::CITEMTYPE_TNBCH},
};
const Wallet::UnitNameReverse Wallet::unit_lookup_{
    reverse_unit_map(unit_of_account_)};

Wallet::Wallet(const api::internal::Core& core)
    : api_(core)
    , context_map_()
    , context_map_lock_()
    , account_map_()
    , nym_map_()
    , server_map_()
    , unit_map_()
    , issuer_map_()
    , account_map_lock_()
    , nym_map_lock_()
    , server_map_lock_()
    , unit_map_lock_()
    , issuer_map_lock_()
    , peer_map_lock_()
    , peer_lock_()
    , nymfile_map_lock_()
    , nymfile_lock_()
#if OT_CASH
    , purse_lock_()
    , purse_id_lock_()
#endif
    , account_publisher_(api_.ZeroMQ().PublishSocket())
    , issuer_publisher_(api_.ZeroMQ().PublishSocket())
    , nym_publisher_(api_.ZeroMQ().PublishSocket())
    , server_publisher_(api_.ZeroMQ().PublishSocket())
    , peer_reply_publisher_(api_.ZeroMQ().PublishSocket())
    , peer_request_publisher_(api_.ZeroMQ().PublishSocket())
    , dht_nym_requester_{api_.ZeroMQ().RequestSocket()}
    , dht_server_requester_{api_.ZeroMQ().RequestSocket()}
    , dht_unit_requester_{api_.ZeroMQ().RequestSocket()}
    , find_nym_(api_.ZeroMQ().PushSocket(
          opentxs::network::zeromq::socket::Socket::Direction::Connect))
{
    account_publisher_->Start(api_.Endpoints().AccountUpdate());
    issuer_publisher_->Start(api_.Endpoints().IssuerUpdate());
    nym_publisher_->Start(api_.Endpoints().NymDownload());
    server_publisher_->Start(api_.Endpoints().ServerUpdate());
    peer_reply_publisher_->Start(api_.Endpoints().PeerReplyUpdate());
    peer_request_publisher_->Start(api_.Endpoints().PeerRequestUpdate());
    dht_nym_requester_->Start(api_.Endpoints().DhtRequestNym());
    dht_server_requester_->Start(api_.Endpoints().DhtRequestServer());
    dht_unit_requester_->Start(api_.Endpoints().DhtRequestUnit());
    find_nym_->Start(api_.Endpoints().FindNym());
}

Wallet::AccountLock& Wallet::account(
    const Lock& lock,
    const Identifier& account,
    const bool create) const
{
    OT_ASSERT(CheckLock(lock, account_map_lock_))

    auto& row = account_map_[account];
    auto& [rowMutex, pAccount] = row;

    if (pAccount) {
        LogVerbose(OT_METHOD)(__FUNCTION__)(": Account ")(account)(
            " already exists in map.")
            .Flush();

        return row;
    }

    eLock rowLock(rowMutex);
    // What if more than one thread tries to create the same row at the same
    // time? One thread will construct the Account object and the other(s) will
    // block until the lock is obtained. Therefore this check is necessary to
    // avoid creating the same account twice.
    if (pAccount) { return row; }

    std::string serialized{""};
    std::string alias{""};
    const auto loaded =
        api_.Storage().Load(account.str(), serialized, alias, true);

    if (loaded) {
        LogVerbose(OT_METHOD)(__FUNCTION__)(": Account ")(account)(
            " loaded from storage.")
            .Flush();
        pAccount.reset(account_factory(account, alias, serialized));

        OT_ASSERT(pAccount);
    } else {
        if (false == create) {
            LogDetail(OT_METHOD)(__FUNCTION__)(": Trying to load account ")(
                account)(" via legacy method.")
                .Flush();
            const auto legacy = load_legacy_account(account, rowLock, row);

            if (legacy) { return row; }

            throw std::out_of_range("Unable to load account from storage");
        }
    }

    return row;
}

SharedAccount Wallet::Account(const Identifier& accountID) const
{
    Lock mapLock(account_map_lock_);

    try {
        auto& row = account(mapLock, accountID, false);
        // WTF clang? This is perfectly valid c++17. Fix your shit.
        // auto& [rowMutex, pAccount] = row;
        auto& rowMutex = std::get<0>(row);
        auto& pAccount = std::get<1>(row);

        if (pAccount) { return SharedAccount(pAccount.get(), rowMutex); }
    } catch (...) {

        return {};
    }

    return {};
}

std::string Wallet::account_alias(
    const std::string& accountID,
    const std::string& hint) const
{
    if (false == hint.empty()) { return hint; }

    return api_.Storage().AccountAlias(Identifier::Factory(accountID));
}

opentxs::Account* Wallet::account_factory(
    const Identifier& accountID,
    const std::string& alias,
    const std::string& serialized) const
{
    auto strContract = String::Factory(), strFirstLine = String::Factory();
    const bool bProcessed = Contract::DearmorAndTrim(
        String::Factory(serialized.c_str()), strContract, strFirstLine);

    if (false == bProcessed) {
        LogOutput(OT_METHOD)(__FUNCTION__)(
            ": Failed to dearmor serialized account.")
            .Flush();

        return nullptr;
    }

    const auto owner = api_.Storage().AccountOwner(accountID);
    const auto notary = api_.Storage().AccountServer(accountID);

    std::unique_ptr<opentxs::Account> pAccount{
        new opentxs::Account{api_, owner, accountID, notary}};

    if (false == bool(pAccount)) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Failed to create account.")
            .Flush();

        return nullptr;
    }

    auto& account = *pAccount;

    if (account.GetNymID() != owner) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Nym id (")(account.GetNymID())(
            ") does not match expect value (")(owner)(")")
            .Flush();
        account.SetNymID(owner);
    }

    if (account.GetRealAccountID() != accountID) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Account id (")(
            account.GetRealAccountID())(") does not match expect value (")(
            accountID)(")")
            .Flush();
        account.SetRealAccountID(accountID);
    }

    if (account.GetPurportedAccountID() != accountID) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Purported account id (")(
            account.GetPurportedAccountID())(") does not match expect value (")(
            accountID)(")")
            .Flush();
        account.SetPurportedAccountID(accountID);
    }

    if (account.GetRealNotaryID() != notary) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Notary id (")(
            account.GetRealNotaryID())(") does not match expect value (")(
            notary)(")")
            .Flush();
        account.SetRealNotaryID(notary);
    }

    if (account.GetPurportedNotaryID() != notary) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Purported notary id (")(
            account.GetPurportedNotaryID())(") does not match expect value (")(
            notary)(")")
            .Flush();
        account.SetPurportedNotaryID(notary);
    }

    account.SetLoadInsecure();
    auto deserialized = account.LoadContractFromString(strContract);

    if (false == deserialized) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Failed to deserialize account.")
            .Flush();

        return nullptr;
    }

    const auto signerID = api_.Storage().AccountSigner(accountID);

    if (signerID->empty()) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Unknown signer nym.").Flush();

        return nullptr;
    }

    const auto signerNym = Nym(signerID);

    if (false == bool(signerNym)) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Unable to load signer nym.")
            .Flush();

        return nullptr;
    }

    if (false == account.VerifySignature(*signerNym)) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Invalid signature.").Flush();

        return nullptr;
    }

    account.SetAlias(alias.c_str());

    return pAccount.release();
}

OTIdentifier Wallet::AccountPartialMatch(const std::string& hint) const
{
    const auto list = api_.Storage().AccountList();

    for (const auto& [id, alias] : list) {
        if (0 == id.compare(0, hint.size(), hint)) {

            return Identifier::Factory(id);
        }

        if (0 == alias.compare(0, hint.size(), hint)) {

            return Identifier::Factory(alias);
        }
    }

    return Identifier::Factory();
}

auto Wallet::BasketContract(
    const identifier::UnitDefinition& id,
    const std::chrono::milliseconds& timeout) const noexcept(false)
    -> OTBasketContract
{
    UnitDefinition(id, timeout);

    Lock mapLock(unit_map_lock_);
    auto it = unit_map_.find(id.str());

    if (unit_map_.end() == it) {
        throw std::runtime_error("Basket contract ID not found");
    }

    auto output = std::dynamic_pointer_cast<contract::unit::Basket>(it->second);

    if (output) {

        return OTBasketContract{std::move(output)};
    } else {
        throw std::runtime_error("Unit definition is not a basket contract");
    }
}

ExclusiveAccount Wallet::CreateAccount(
    const identifier::Nym& ownerNymID,
    const identifier::Server& notaryID,
    const identifier::UnitDefinition& instrumentDefinitionID,
    const identity::Nym& signer,
    Account::AccountType type,
    TransactionNumber stash,
    const PasswordPrompt& reason) const
{
    Lock mapLock(account_map_lock_);

    try {
        const auto contract = UnitDefinition(instrumentDefinitionID);
        std::unique_ptr<opentxs::Account> newAccount(
            opentxs::Account::GenerateNewAccount(
                api_,
                signer.ID(),
                notaryID,
                signer,
                ownerNymID,
                instrumentDefinitionID,
                reason,
                type,
                stash));

        OT_ASSERT(newAccount)

        const auto& accountID = newAccount->GetRealAccountID();
        auto& row = account(mapLock, accountID, true);
        // WTF clang? This is perfectly valid c++17. Fix your shit.
        // auto& [rowMutex, pAccount] = row;
        auto& rowMutex = std::get<0>(row);
        auto& pAccount = std::get<1>(row);

        if (pAccount) {
            LogOutput(OT_METHOD)(__FUNCTION__)(": Account already exists.")
                .Flush();

            return {};
        } else {
            pAccount.reset(newAccount.release());

            OT_ASSERT(pAccount)

            const auto id = accountID.str();
            pAccount->SetNymID(ownerNymID);
            pAccount->SetPurportedAccountID(accountID);
            pAccount->SetRealNotaryID(notaryID);
            pAccount->SetPurportedNotaryID(notaryID);
            auto serialized = String::Factory();
            pAccount->SaveContractRaw(serialized);
            const auto saved = api_.Storage().Store(
                id,
                serialized->Get(),
                "",
                ownerNymID,
                signer.ID(),
                contract->Nym()->ID(),
                notaryID,
                instrumentDefinitionID,
                extract_unit(instrumentDefinitionID));

            OT_ASSERT(saved)

            std::function<void(
                std::unique_ptr<opentxs::Account>&, eLock&, bool)>
                callback = [this, id, &reason](
                               std::unique_ptr<opentxs::Account>& in,
                               eLock& lock,
                               bool success) -> void {
                this->save(reason, id, in, lock, success);
            };

            return ExclusiveAccount(&pAccount, rowMutex, callback);
        }
    } catch (...) {

        return {};
    }
}

bool Wallet::DeleteAccount(const Identifier& accountID) const
{
    Lock mapLock(account_map_lock_);

    try {
        auto& row = account(mapLock, accountID, false);
        // WTF clang? This is perfectly valid c++17. Fix your shit.
        // auto& [rowMutex, pAccount] = row;
        auto& rowMutex = std::get<0>(row);
        auto& pAccount = std::get<1>(row);
        eLock lock(rowMutex);

        if (pAccount) {
            const auto deleted = api_.Storage().DeleteAccount(accountID.str());

            if (deleted) {
                pAccount.reset();

                return true;
            }
        }
    } catch (...) {

        return false;
    }

    return false;
}

SharedAccount Wallet::IssuerAccount(
    const identifier::UnitDefinition& unitID) const
{
    const auto accounts = api_.Storage().AccountsByContract(unitID);
    Lock mapLock(account_map_lock_);

    try {
        for (const auto& accountID : accounts) {
            auto& row = account(mapLock, accountID, false);
            // WTF clang? This is perfectly valid c++17. Fix your shit.
            // auto& [rowMutex, pAccount] = row;
            auto& rowMutex = std::get<0>(row);
            auto& pAccount = std::get<1>(row);

            if (pAccount) {
                if (pAccount->IsIssuer()) {

                    return SharedAccount(pAccount.get(), rowMutex);
                }
            }
        }
    } catch (...) {

        return {};
    }

    return {};
}

ExclusiveAccount Wallet::mutable_Account(
    const Identifier& accountID,
    const PasswordPrompt& reason,
    const AccountCallback callback) const
{
    Lock mapLock(account_map_lock_);

    try {
        auto& [rowMutex, pAccount] = account(mapLock, accountID, false);
        const auto id = accountID.str();

        if (pAccount) {
            std::function<void(
                std::unique_ptr<opentxs::Account>&, eLock&, bool)>
                save = [this, id, &reason](
                           std::unique_ptr<opentxs::Account>& in,
                           eLock& lock,
                           bool success) -> void {
                this->save(reason, id, in, lock, success);
            };

            return ExclusiveAccount(&pAccount, rowMutex, save, callback);
        }
    } catch (...) {

        return {};
    }

    return {};
}

bool Wallet::UpdateAccount(
    const Identifier& accountID,
    const opentxs::ServerContext& context,
    const String& serialized,
    const PasswordPrompt& reason) const
{
    return UpdateAccount(accountID, context, serialized, "", reason);
}

bool Wallet::UpdateAccount(
    const Identifier& accountID,
    const opentxs::ServerContext& context,
    const String& serialized,
    const std::string& label,
    const PasswordPrompt& reason) const
{
    Lock mapLock(account_map_lock_);
    auto& row = account(mapLock, accountID, true);
    // WTF clang? This is perfectly valid c++17. Fix your shit.
    // auto& [rowMutex, pAccount] = row;
    auto& rowMutex = std::get<0>(row);
    auto& pAccount = std::get<1>(row);
    eLock rowLock(rowMutex);
    mapLock.unlock();
    const auto& localNym = *context.Nym();
    std::unique_ptr<opentxs::Account> newAccount{nullptr};
    newAccount.reset(
        new opentxs::Account(api_, localNym.ID(), accountID, context.Server()));

    if (false == bool(newAccount)) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Unable to construct account.")
            .Flush();

        return false;
    }

    if (false == newAccount->LoadContractFromString(serialized)) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Unable to deserialize account.")
            .Flush();

        return false;
    }

    if (false == newAccount->VerifyAccount(context.RemoteNym())) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Unable to verify account.")
            .Flush();

        return false;
    }

    if (localNym.ID() != newAccount->GetNymID()) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Wrong nym on account.").Flush();

        return false;
    }

    if (context.Server() != newAccount->GetRealNotaryID()) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Wrong server on account.")
            .Flush();

        return false;
    }

    newAccount->ReleaseSignatures();

    if (false == newAccount->SignContract(localNym, reason)) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Unable to sign account.").Flush();

        return false;
    }

    if (false == newAccount->SaveContract()) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Unable to serialize account.")
            .Flush();

        return false;
    }

    pAccount.reset(newAccount.release());

    OT_ASSERT(pAccount)

    const auto& unitID = pAccount->GetInstrumentDefinitionID();

    try {
        const auto contract = UnitDefinition(unitID);
        auto raw = String::Factory();
        auto saved = pAccount->SaveContractRaw(raw);

        if (false == saved) {
            LogOutput(OT_METHOD)(__FUNCTION__)(
                ": Unable to serialized account.")
                .Flush();

            return false;
        }

        const auto alias = account_alias(accountID.str(), label);
        saved = api_.Storage().Store(
            accountID.str(),
            raw->Get(),
            alias,
            localNym.ID(),
            localNym.ID(),
            contract->Nym()->ID(),
            context.Server(),
            unitID,
            extract_unit(contract));

        if (false == saved) {
            LogOutput(OT_METHOD)(__FUNCTION__)(": Unable to save account.")
                .Flush();

            return false;
        }

        pAccount->SetAlias(alias);
        const auto balance = pAccount->GetBalance();
        auto message = opentxs::network::zeromq::Message::Factory();
        message->AddFrame(accountID.str());
        message->AddFrame(Data::Factory(&balance, sizeof(balance)));
        account_publisher_->Send(message);

        return true;
    } catch (...) {
        LogOutput(OT_METHOD)(__FUNCTION__)(
            ": Unable to load unit definition contract ")(unitID)(".")
            .Flush();

        return false;
    }
}

proto::ContactItemType Wallet::CurrencyTypeBasedOnUnitType(
    const identifier::UnitDefinition& contractID) const
{
    return extract_unit(contractID);
}

proto::ContactItemType Wallet::extract_unit(
    const identifier::UnitDefinition& contractID) const
{
    try {
        const auto contract = UnitDefinition(contractID);

        return extract_unit(contract);
    } catch (...) {
        LogOutput(OT_METHOD)(__FUNCTION__)(
            ": Unable to load unit definition contract ")(contractID)(".")
            .Flush();

        return proto::CITEMTYPE_UNKNOWN;
    }
}

proto::ContactItemType Wallet::extract_unit(
    const contract::Unit& contract) const
{
    try {
        if (contract.Version() < 2) {
            return unit_of_account_.at(contract.TLA());
        }

        return contract.UnitOfAccount();
    } catch (...) {

        return proto::CITEMTYPE_UNKNOWN;
    }
}

#if OT_CASH
std::mutex& Wallet::get_purse_lock(
    const identifier::Nym& nym,
    const identifier::Server& server,
    const identifier::UnitDefinition& unit) const
{
    Lock lock(purse_lock_);
    const PurseID id{nym, server, unit};

    return purse_id_lock_[id];
}
#endif

std::shared_ptr<opentxs::Context> Wallet::context(
    const identifier::Nym& localNymID,
    const identifier::Nym& remoteNymID) const
{
    const std::string local = localNymID.str();
    const std::string remote = remoteNymID.str();
    const ContextID context = {local, remote};
    auto it = context_map_.find(context);
    const bool inMap = (it != context_map_.end());

    if (inMap) { return it->second; }

    // Load from storage, if it exists.
    std::shared_ptr<proto::Context> serialized;
    const bool loaded = api_.Storage().Load(
        localNymID.str(), remoteNymID.str(), serialized, true);

    if (!loaded) { return nullptr; }

    if (local != serialized->localnym()) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Incorrect localnym in protobuf.")
            .Flush();

        return nullptr;
    }

    if (remote != serialized->remotenym()) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Incorrect localnym in protobuf.")
            .Flush();

        return nullptr;
    }

    auto& entry = context_map_[context];

    // Obtain nyms.
    const auto localNym = Nym(localNymID);
    const auto remoteNym = Nym(remoteNymID);

    if (!localNym) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Unable to load local nym.")
            .Flush();

        return nullptr;
    }

    if (!remoteNym) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Unable to load remote nym.")
            .Flush();

        return nullptr;
    }

    switch (serialized->type()) {
        case proto::CONSENSUSTYPE_SERVER: {
            instantiate_server_context(*serialized, localNym, remoteNym, entry);
        } break;
        case proto::CONSENSUSTYPE_CLIENT: {
            instantiate_client_context(*serialized, localNym, remoteNym, entry);
        } break;
        default: {
            return nullptr;
        }
    }

    OT_ASSERT(entry);

    const bool valid = entry->Validate();

    if (!valid) {
        context_map_.erase(context);

        LogOutput(OT_METHOD)(__FUNCTION__)(": Invalid signature on context.")
            .Flush();

        OT_FAIL
    }

    return entry;
}

std::shared_ptr<const opentxs::ClientContext> Wallet::ClientContext(
    const identifier::Nym& remoteNymID) const
{
    // Overridden in appropriate child class.
    OT_FAIL;
}

std::shared_ptr<const opentxs::ServerContext> Wallet::ServerContext(
    const identifier::Nym& localNymID,
    const Identifier& remoteID) const
{
    // Overridden in appropriate child class.
    OT_FAIL;
}

Editor<opentxs::ClientContext> Wallet::mutable_ClientContext(
    const identifier::Nym& remoteNymID,
    const PasswordPrompt& reason) const
{
    // Overridden in appropriate child class.
    OT_FAIL;
}

Editor<opentxs::ServerContext> Wallet::mutable_ServerContext(
    const identifier::Nym& localNymID,
    const Identifier& remoteID,
    const PasswordPrompt& reason) const
{
    // Overridden in appropriate child class.
    OT_FAIL;
}

bool Wallet::ImportAccount(std::unique_ptr<opentxs::Account>& imported) const
{
    if (false == bool(imported)) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Invalid account.").Flush();

        return false;
    }

    const auto& accountID = imported->GetRealAccountID();
    Lock mapLock(account_map_lock_);

    try {
        auto& row = account(mapLock, accountID, true);
        // WTF clang? This is perfectly valid c++17. Fix your shit.
        // auto& [rowMutex, pAccount] = row;
        auto& rowMutex = std::get<0>(row);
        auto& pAccount = std::get<1>(row);
        eLock rowLock(rowMutex);
        mapLock.unlock();

        if (pAccount) {
            LogOutput(OT_METHOD)(__FUNCTION__)(": Account already exists.")
                .Flush();

            return false;
        }

        pAccount.reset(imported.release());

        OT_ASSERT(pAccount)

        const auto& contractID = pAccount->GetInstrumentDefinitionID();

        try {
            const auto contract = UnitDefinition(contractID);
            auto serialized = String::Factory();
            auto alias = String::Factory();
            pAccount->SaveContractRaw(serialized);
            pAccount->GetName(alias);
            const auto saved = api_.Storage().Store(
                accountID.str(),
                serialized->Get(),
                alias->Get(),
                pAccount->GetNymID(),
                pAccount->GetNymID(),
                contract->Nym()->ID(),
                pAccount->GetRealNotaryID(),
                contractID,
                extract_unit(contract));

            if (false == saved) {
                LogOutput(OT_METHOD)(__FUNCTION__)(": Failed to save account.")
                    .Flush();
                imported.reset(pAccount.release());

                return false;
            }

            return true;
        } catch (...) {
            LogOutput(OT_METHOD)(__FUNCTION__)(
                ": Unable to load unit definition.")
                .Flush();
            imported.reset(pAccount.release());

            return false;
        }
    } catch (...) {
    }

    LogOutput(OT_METHOD)(__FUNCTION__)(": Unable to import account.").Flush();

    return false;
}

std::set<OTNymID> Wallet::IssuerList(const identifier::Nym& nymID) const
{
    std::set<OTNymID> output{};
    auto list = api_.Storage().IssuerList(nymID.str());

    for (const auto& it : list) {
        output.emplace(identifier::Nym::Factory(it.first));
    }

    return output;
}

std::shared_ptr<const api::client::Issuer> Wallet::Issuer(
    const identifier::Nym& nymID,
    const identifier::Nym& issuerID) const
{
    auto& [lock, pIssuer] = issuer(nymID, issuerID, false);
    const auto& notUsed [[maybe_unused]] = lock;

    return pIssuer;
}

Editor<api::client::Issuer> Wallet::mutable_Issuer(
    const identifier::Nym& nymID,
    const identifier::Nym& issuerID) const
{
    auto& [lock, pIssuer] = issuer(nymID, issuerID, true);

    OT_ASSERT(pIssuer);

    std::function<void(api::client::Issuer*, const Lock&)> callback =
        [=](api::client::Issuer* in, const Lock& lock) -> void {
        this->save(lock, in);
    };

    return Editor<api::client::Issuer>(lock, pIssuer.get(), callback);
}

Wallet::IssuerLock& Wallet::issuer(
    const identifier::Nym& nymID,
    const identifier::Nym& issuerID,
    const bool create) const
{
    Lock lock(issuer_map_lock_);
    auto& output = issuer_map_[{nymID, issuerID}];
    auto& [issuerMutex, pIssuer] = output;
    const auto& notUsed [[maybe_unused]] = issuerMutex;

    if (pIssuer) { return output; }

    std::shared_ptr<proto::Issuer> serialized{nullptr};
    const bool loaded =
        api_.Storage().Load(nymID.str(), issuerID.str(), serialized, true);

    if (loaded) {
        OT_ASSERT(serialized)

        pIssuer.reset(opentxs::Factory::Issuer(*this, nymID, *serialized));

        OT_ASSERT(pIssuer)

        return output;
    }

    if (create) {
        pIssuer.reset(opentxs::Factory::Issuer(*this, nymID, issuerID));

        OT_ASSERT(pIssuer);

        save(lock, pIssuer.get());
    }

    return output;
}

bool Wallet::IsLocalNym(const std::string& id) const
{
    return api_.Storage().LocalNyms().count(id);
}

std::size_t Wallet::LocalNymCount() const
{
    return api_.Storage().LocalNyms().size();
}

std::set<OTNymID> Wallet::LocalNyms() const
{
    const std::set<std::string> ids = api_.Storage().LocalNyms();

    std::set<OTNymID> nymIds;
    std::transform(
        ids.begin(),
        ids.end(),
        std::inserter(nymIds, nymIds.end()),
        [](std::string nym) -> OTNymID {
            return identifier::Nym::Factory(nym);
        });

    return nymIds;
}

Nym_p Wallet::Nym(
    const identifier::Nym& id,
    const std::chrono::milliseconds& timeout) const
{
    const std::string nym = id.str();
    Lock mapLock(nym_map_lock_);
    bool inMap = (nym_map_.find(nym) != nym_map_.end());
    bool valid = false;

    if (!inMap) {
        auto pSerialized = std::shared_ptr<proto::Nym>{};
        auto alias = std::string{};
        bool loaded = api_.Storage().Load(nym, pSerialized, alias, true);

        if (loaded) {
            OT_ASSERT(pSerialized)

            const auto& serialized = *pSerialized;
            auto& pNym = nym_map_[nym].second;
            pNym.reset(opentxs::Factory::Nym(api_, serialized, alias));

            if (pNym && pNym->CompareID(id)) {
                valid = pNym->VerifyPseudonym();
                pNym->SetAliasStartup(alias);
            } else {
                nym_map_.erase(nym);
            }
        } else {
            dht_nym_requester_->Send(nym);

            if (timeout > std::chrono::milliseconds(0)) {
                mapLock.unlock();
                auto start = std::chrono::high_resolution_clock::now();
                auto end = start + timeout;
                const auto interval = std::chrono::milliseconds(100);

                while (std::chrono::high_resolution_clock::now() < end) {
                    std::this_thread::sleep_for(interval);
                    mapLock.lock();
                    bool found = (nym_map_.find(nym) != nym_map_.end());
                    mapLock.unlock();

                    if (found) { break; }
                }

                return Nym(id);  // timeout of zero prevents infinite
                                 // recursion
            }
        }
    } else {
        auto& pNym = nym_map_[nym].second;
        if (pNym) { valid = pNym->VerifyPseudonym(); }
    }

    if (valid) { return nym_map_[nym].second; }

    return nullptr;
}

Nym_p Wallet::Nym(const proto::Nym& serialized) const
{
    const auto& id = serialized.nymid();
    const auto nymID = identifier::Nym::Factory(id);

    if (nymID->empty()) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Invalid nym ID.").Flush();

        return {};
    }

    auto existing = Nym(nymID);

    if (existing && (existing->Revision() >= serialized.revision())) {
        LogDetail(OT_METHOD)(__FUNCTION__)(
            ": Incoming nym is not newer than existing nym.")
            .Flush();

        return existing;
    } else {
        auto pCandidate = std::unique_ptr<identity::internal::Nym>{
            opentxs::Factory::Nym(api_, serialized, "")};

        if (false == bool(pCandidate)) { return {}; }

        auto& candidate = *pCandidate;

        if (false == candidate.CompareID(nymID)) { return existing; }

        if (candidate.VerifyPseudonym()) {
            LogDetail(OT_METHOD)(__FUNCTION__)(": Saving updated nym ")(id)
                .Flush();
            candidate.WriteCredentials();
            SaveCredentialIDs(candidate);
            Lock mapLock(nym_map_lock_);
            auto& mapNym = nym_map_[id].second;
            // TODO update existing nym rather than destroying it
            mapNym.reset(pCandidate.release());
            nym_publisher_->Send(id);

            return mapNym;
        } else {
            LogOutput(OT_METHOD)(__FUNCTION__)(": Incoming nym is not valid.")
                .Flush();
        }
    }

    return existing;
}

Nym_p Wallet::Nym(
    const PasswordPrompt& reason,
    const std::string name,
    const NymParameters& parameters,
    const proto::ContactItemType type) const
{
    std::shared_ptr<identity::internal::Nym> pNym(
        opentxs::Factory::Nym(api_, parameters, type, name, reason));

    if (false == bool(pNym)) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Failed to create nym").Flush();

        return {};
    }

    auto& nym = *pNym;

    if (nym.VerifyPseudonym()) {
        nym.SetAlias(name);

        {
            Lock mapLock(nym_map_lock_);
            auto it = nym_map_.find(nym.ID().str());

            if (nym_map_.end() != it) { return it->second.second; }
        }

        if (SaveCredentialIDs(nym)) {
            nym_to_contact(nym, name);

            {
                auto nymfile = mutable_nymfile(pNym, pNym, nym.ID(), reason);
            }

            Lock mapLock(nym_map_lock_);
            auto& pMapNym = nym_map_[nym.ID().str()].second;
            pMapNym = pNym;

            return std::move(pNym);
        } else {
            LogOutput(OT_METHOD)(__FUNCTION__)(": Failed to save credentials")
                .Flush();

            return {};
        }
    } else {

        return {};
    }
}

NymData Wallet::mutable_Nym(
    const identifier::Nym& id,
    const PasswordPrompt& reason) const
{
    const std::string nym = id.str();
    auto exists = Nym(id);

    if (false == bool(exists)) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Nym ")(nym)(" not found.")
            .Flush();
    }

    Lock mapLock(nym_map_lock_);
    auto it = nym_map_.find(nym);

    if (nym_map_.end() == it) { OT_FAIL }

    std::function<void(NymData*, Lock&)> callback = [&](NymData* nymData,
                                                        Lock& lock) -> void {
        this->save(nymData, lock);
    };

    return NymData(
        api_.Factory(), it->second.first, it->second.second, callback);
}

std::unique_ptr<const opentxs::NymFile> Wallet::Nymfile(
    const identifier::Nym& id,
    const PasswordPrompt& reason) const
{
    Lock lock(nymfile_lock(id));
    const auto targetNym = Nym(id);
    const auto signerNym = signer_nym(id);

    if (false == bool(targetNym)) { return {}; }
    if (false == bool(signerNym)) { return {}; }

    auto nymfile = std::unique_ptr<opentxs::internal::NymFile>(
        opentxs::Factory::NymFile(api_, targetNym, signerNym));

    OT_ASSERT(nymfile)

    if (false == nymfile->LoadSignedNymFile(reason)) {
        LogOutput(OT_METHOD)(__FUNCTION__)(
            ": Failure calling load_signed_nymfile: ")(id)(".")
            .Flush();

        return {};
    }

    return std::move(nymfile);
}

Editor<opentxs::NymFile> Wallet::mutable_Nymfile(
    const identifier::Nym& id,
    const PasswordPrompt& reason) const
{
    const auto targetNym = Nym(id);
    const auto signerNym = signer_nym(id);

    return mutable_nymfile(targetNym, signerNym, id, reason);
}

Editor<opentxs::NymFile> Wallet::mutable_nymfile(
    const Nym_p& targetNym,
    const Nym_p& signerNym,
    const identifier::Nym& id,
    const PasswordPrompt& reason) const
{
    auto nymfile = std::unique_ptr<opentxs::internal::NymFile>(
        opentxs::Factory::NymFile(api_, targetNym, signerNym));

    OT_ASSERT(nymfile)

    if (false == nymfile->LoadSignedNymFile(reason)) {
        nymfile->SaveSignedNymFile(reason);
    }

    using EditorType = Editor<opentxs::NymFile>;
    EditorType::LockedSave callback = [&](opentxs::NymFile* in,
                                          Lock& lock) -> void {
        this->save(reason, in, lock);
    };
    EditorType::OptionalCallback deleter = [](const opentxs::NymFile& in) {
        auto* p = &const_cast<opentxs::NymFile&>(in);
        delete p;
    };

    return EditorType(nymfile_lock(id), nymfile.release(), callback, deleter);
}

std::mutex& Wallet::nymfile_lock(const identifier::Nym& nymID) const
{
    Lock map_lock(nymfile_map_lock_);
    auto& output = nymfile_lock_[Identifier::Factory(nymID)];
    map_lock.unlock();

    return output;
}

Nym_p Wallet::NymByIDPartialMatch(const std::string& partialId) const
{
    Lock mapLock(nym_map_lock_);
    bool inMap = (nym_map_.find(partialId) != nym_map_.end());
    bool valid = false;

    if (!inMap) {
        for (auto& it : nym_map_) {
            if (it.first.compare(0, partialId.length(), partialId) == 0)
                if (it.second.second->VerifyPseudonym())
                    return it.second.second;
        }
        for (auto& it : nym_map_) {
            if (it.second.second->Alias().compare(
                    0, partialId.length(), partialId) == 0)
                if (it.second.second->VerifyPseudonym())
                    return it.second.second;
        }
    } else {
        auto& pNym = nym_map_[partialId].second;
        if (pNym) { valid = pNym->VerifyPseudonym(); }
    }

    if (valid) { return nym_map_[partialId].second; }

    return nullptr;
}

ObjectList Wallet::NymList() const { return api_.Storage().NymList(); }

bool Wallet::NymNameByIndex(const std::size_t index, String& name) const
{
    std::set<std::string> nymNames = api_.Storage().LocalNyms();

    if (index < nymNames.size()) {
        std::size_t idx{0};
        for (auto& nymName : nymNames) {
            if (idx == index) {
                name.Set(String::Factory(nymName));

                return true;
            }

            ++idx;
        }
    }

    return false;
}

std::mutex& Wallet::peer_lock(const std::string& nymID) const
{
    Lock map_lock(peer_map_lock_);
    auto& output = peer_lock_[nymID];
    map_lock.unlock();

    return output;
}

std::shared_ptr<proto::PeerReply> Wallet::PeerReply(
    const identifier::Nym& nym,
    const Identifier& reply,
    const StorageBox& box) const
{
    const std::string nymID = nym.str();
    Lock lock(peer_lock(nymID));
    std::shared_ptr<proto::PeerReply> output;

    api_.Storage().Load(nymID, reply.str(), box, output, true);

    return output;
}

bool Wallet::PeerReplyComplete(
    const identifier::Nym& nym,
    const Identifier& replyID) const
{
    const std::string nymID = nym.str();
    Lock lock(peer_lock(nymID));
    std::shared_ptr<proto::PeerReply> reply;
    const bool haveReply = api_.Storage().Load(
        nymID, replyID.str(), StorageBox::SENTPEERREPLY, reply, false);

    if (!haveReply) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Sent reply not found.").Flush();

        return false;
    }

    // This reply may have been loaded by request id.
    const auto& realReplyID = reply->id();

    const bool savedReply =
        api_.Storage().Store(*reply, nymID, StorageBox::FINISHEDPEERREPLY);

    if (!savedReply) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Failed to save finished reply.")
            .Flush();

        return false;
    }

    const bool removedReply = api_.Storage().RemoveNymBoxItem(
        nymID, StorageBox::SENTPEERREPLY, realReplyID);

    if (!removedReply) {
        LogOutput(OT_METHOD)(__FUNCTION__)(
            ": Failed to delete finished reply from sent box.")
            .Flush();
    }

    return removedReply;
}

bool Wallet::PeerReplyCreate(
    const identifier::Nym& nym,
    const proto::PeerRequest& request,
    const proto::PeerReply& reply) const
{
    const std::string nymID = nym.str();
    Lock lock(peer_lock(nymID));

    if (reply.cookie() != request.id()) {
        LogOutput(OT_METHOD)(__FUNCTION__)(
            ": Reply cookie does not match request id.")
            .Flush();

        return false;
    }

    if (reply.type() != request.type()) {
        LogOutput(OT_METHOD)(__FUNCTION__)(
            ": Reply type does not match request type.")
            .Flush();

        return false;
    }

    const bool createdReply =
        api_.Storage().Store(reply, nymID, StorageBox::SENTPEERREPLY);

    if (!createdReply) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Failed to save sent reply.")
            .Flush();

        return false;
    }

    const bool processedRequest =
        api_.Storage().Store(request, nymID, StorageBox::PROCESSEDPEERREQUEST);

    if (!processedRequest) {
        LogOutput(OT_METHOD)(__FUNCTION__)(
            ": Failed to save processed request.")
            .Flush();

        return false;
    }

    const bool movedRequest = api_.Storage().RemoveNymBoxItem(
        nymID, StorageBox::INCOMINGPEERREQUEST, request.id());

    if (!processedRequest) {
        LogOutput(OT_METHOD)(__FUNCTION__)(
            ": Failed to delete processed request from incoming box.")
            .Flush();
    }

    return movedRequest;
}

bool Wallet::PeerReplyCreateRollback(
    const identifier::Nym& nym,
    const Identifier& request,
    const Identifier& reply) const
{
    const std::string nymID = nym.str();
    Lock lock(peer_lock(nymID));
    const std::string requestID = request.str();
    const std::string replyID = reply.str();
    std::shared_ptr<proto::PeerRequest> requestItem;
    bool output = true;
    time_t notUsed = 0;
    const bool loadedRequest = api_.Storage().Load(
        nymID,
        requestID,
        StorageBox::PROCESSEDPEERREQUEST,
        requestItem,
        notUsed);

    if (loadedRequest) {
        const bool requestRolledBack = api_.Storage().Store(
            *requestItem, nymID, StorageBox::INCOMINGPEERREQUEST);

        if (requestRolledBack) {
            const bool purgedRequest = api_.Storage().RemoveNymBoxItem(
                nymID, StorageBox::PROCESSEDPEERREQUEST, requestID);
            if (!purgedRequest) {
                LogOutput(OT_METHOD)(__FUNCTION__)(
                    ": Failed to delete request from processed box.")
                    .Flush();
                output = false;
            }
        } else {
            LogOutput(OT_METHOD)(__FUNCTION__)(
                ": Failed to save request to incoming box.")
                .Flush();
            output = false;
        }
    } else {
        LogOutput(OT_METHOD)(__FUNCTION__)(
            ": Did not find the request in the processed box.")
            .Flush();
        output = false;
    }

    const bool removedReply = api_.Storage().RemoveNymBoxItem(
        nymID, StorageBox::SENTPEERREPLY, replyID);

    if (!removedReply) {
        LogOutput(OT_METHOD)(__FUNCTION__)(
            ": Failed to delete reply from sent box.")
            .Flush();
        output = false;
    }

    return output;
}

ObjectList Wallet::PeerReplySent(const identifier::Nym& nym) const
{
    const std::string nymID = nym.str();
    Lock lock(peer_lock(nymID));

    return api_.Storage().NymBoxList(nymID, StorageBox::SENTPEERREPLY);
}

ObjectList Wallet::PeerReplyIncoming(const identifier::Nym& nym) const
{
    const std::string nymID = nym.str();
    Lock lock(peer_lock(nymID));

    return api_.Storage().NymBoxList(nymID, StorageBox::INCOMINGPEERREPLY);
}

ObjectList Wallet::PeerReplyFinished(const identifier::Nym& nym) const
{
    const std::string nymID = nym.str();
    Lock lock(peer_lock(nymID));

    return api_.Storage().NymBoxList(nymID, StorageBox::FINISHEDPEERREPLY);
}

ObjectList Wallet::PeerReplyProcessed(const identifier::Nym& nym) const
{
    const std::string nymID = nym.str();
    Lock lock(peer_lock(nymID));

    return api_.Storage().NymBoxList(nymID, StorageBox::PROCESSEDPEERREPLY);
}

bool Wallet::PeerReplyReceive(
    const identifier::Nym& nym,
    const PeerObject& reply) const
{
    if (proto::PEEROBJECT_RESPONSE != reply.Type()) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": This is not a peer reply.")
            .Flush();

        return false;
    }

    if (0 == reply.Request()->Version()) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Null request.").Flush();

        return false;
    }

    if (0 == reply.Reply()->Version()) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Null reply.").Flush();

        return false;
    }

    const std::string nymID = nym.str();
    Lock lock(peer_lock(nymID));
    auto requestID = reply.Request()->ID();

    std::shared_ptr<proto::PeerRequest> request;
    std::time_t notUsed;
    const bool haveRequest = api_.Storage().Load(
        nymID,
        requestID->str(),
        StorageBox::SENTPEERREQUEST,
        request,
        notUsed,
        false);

    if (haveRequest) {
        OT_ASSERT(request);
    } else {
        LogOutput(OT_METHOD)(__FUNCTION__)(
            ": The request for this reply does not exist in the sent box.")
            .Flush();

        return false;
    }

    const proto::PeerReply serialized{reply.Reply()->Contract()};
    const bool receivedReply =
        api_.Storage().Store(serialized, nymID, StorageBox::INCOMINGPEERREPLY);

    if (receivedReply) {
        auto message = opentxs::network::zeromq::Message::Factory();
        message->AddFrame();
        message->AddFrame(nymID);
        message->AddFrame(serialized);
        peer_reply_publisher_->Send(message);
    } else {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Failed to save incoming reply.")
            .Flush();

        return false;
    }

    const bool finishedRequest =
        api_.Storage().Store(*request, nymID, StorageBox::FINISHEDPEERREQUEST);

    if (!finishedRequest) {
        LogOutput(OT_METHOD)(__FUNCTION__)(
            ": Failed to save request to finished box.")
            .Flush();

        return false;
    }

    const bool removedRequest = api_.Storage().RemoveNymBoxItem(
        nymID, StorageBox::SENTPEERREQUEST, requestID->str());

    if (!finishedRequest) {
        LogOutput(OT_METHOD)(__FUNCTION__)(
            ": Failed to delete finished request from sent box.")
            .Flush();
    }

    return removedRequest;
}

std::shared_ptr<proto::PeerRequest> Wallet::PeerRequest(
    const identifier::Nym& nym,
    const Identifier& request,
    const StorageBox& box,
    std::time_t& time) const
{
    const std::string nymID = nym.str();
    Lock lock(peer_lock(nymID));
    std::shared_ptr<proto::PeerRequest> output;

    api_.Storage().Load(nymID, request.str(), box, output, time, true);

    return output;
}

bool Wallet::PeerRequestComplete(
    const identifier::Nym& nym,
    const Identifier& replyID) const
{
    const std::string nymID = nym.str();
    Lock lock(peer_lock(nymID));
    std::shared_ptr<proto::PeerReply> reply;
    const bool haveReply = api_.Storage().Load(
        nymID, replyID.str(), StorageBox::INCOMINGPEERREPLY, reply, false);

    if (!haveReply) {
        LogOutput(OT_METHOD)(__FUNCTION__)(
            ": The reply does not exist in the incoming box.")
            .Flush();

        return false;
    }

    // This reply may have been loaded by request id.
    const auto& realReplyID = reply->id();

    const bool storedReply =
        api_.Storage().Store(*reply, nymID, StorageBox::PROCESSEDPEERREPLY);

    if (!storedReply) {
        LogOutput(OT_METHOD)(__FUNCTION__)(
            ": Failed to save reply to processed box.")
            .Flush();

        return false;
    }

    const bool removedReply = api_.Storage().RemoveNymBoxItem(
        nymID, StorageBox::INCOMINGPEERREPLY, realReplyID);

    if (!removedReply) {
        LogOutput(OT_METHOD)(__FUNCTION__)(
            ": Failed to delete completed reply from incoming box.")
            .Flush();
    }

    return removedReply;
}

bool Wallet::PeerRequestCreate(
    const identifier::Nym& nym,
    const proto::PeerRequest& request) const
{
    const std::string nymID = nym.str();
    Lock lock(peer_lock(nymID));

    return api_.Storage().Store(
        request, nym.str(), StorageBox::SENTPEERREQUEST);
}

bool Wallet::PeerRequestCreateRollback(
    const identifier::Nym& nym,
    const Identifier& request) const
{
    const std::string nymID = nym.str();
    Lock lock(peer_lock(nymID));

    return api_.Storage().RemoveNymBoxItem(
        nym.str(), StorageBox::SENTPEERREQUEST, request.str());
}

bool Wallet::PeerRequestDelete(
    const identifier::Nym& nym,
    const Identifier& request,
    const StorageBox& box) const
{
    switch (box) {
        case StorageBox::SENTPEERREQUEST:
        case StorageBox::INCOMINGPEERREQUEST:
        case StorageBox::FINISHEDPEERREQUEST:
        case StorageBox::PROCESSEDPEERREQUEST: {
            return api_.Storage().RemoveNymBoxItem(
                nym.str(), box, request.str());
        }
        default: {
            return false;
        }
    }
}

ObjectList Wallet::PeerRequestSent(const identifier::Nym& nym) const
{
    const std::string nymID = nym.str();
    Lock lock(peer_lock(nymID));

    return api_.Storage().NymBoxList(nym.str(), StorageBox::SENTPEERREQUEST);
}

ObjectList Wallet::PeerRequestIncoming(const identifier::Nym& nym) const
{
    const std::string nymID = nym.str();
    Lock lock(peer_lock(nymID));

    return api_.Storage().NymBoxList(
        nym.str(), StorageBox::INCOMINGPEERREQUEST);
}

ObjectList Wallet::PeerRequestFinished(const identifier::Nym& nym) const
{
    const std::string nymID = nym.str();
    Lock lock(peer_lock(nymID));

    return api_.Storage().NymBoxList(
        nym.str(), StorageBox::FINISHEDPEERREQUEST);
}

ObjectList Wallet::PeerRequestProcessed(const identifier::Nym& nym) const
{
    const std::string nymID = nym.str();
    Lock lock(peer_lock(nymID));

    return api_.Storage().NymBoxList(
        nym.str(), StorageBox::PROCESSEDPEERREQUEST);
}

bool Wallet::PeerRequestReceive(
    const identifier::Nym& nym,
    const PeerObject& request) const
{
    if (proto::PEEROBJECT_REQUEST != request.Type()) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": This is not a peer request.")
            .Flush();

        return false;
    }

    if (0 == request.Request()->Version()) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Null request.").Flush();

        return false;
    }

    const proto::PeerRequest serialized{request.Request()->Contract()};
    const std::string nymID = nym.str();
    Lock lock(peer_lock(nymID));
    const auto saved = api_.Storage().Store(
        serialized, nymID, StorageBox::INCOMINGPEERREQUEST);

    if (saved) {
        auto message = opentxs::network::zeromq::Message::Factory();
        message->AddFrame();
        message->AddFrame(nymID);
        message->AddFrame(serialized);
        peer_request_publisher_->Send(message);
    }

    return saved;
}

bool Wallet::PeerRequestUpdate(
    const identifier::Nym& nym,
    const Identifier& request,
    const StorageBox& box) const
{
    switch (box) {
        case StorageBox::SENTPEERREQUEST:
        case StorageBox::INCOMINGPEERREQUEST:
        case StorageBox::FINISHEDPEERREQUEST:
        case StorageBox::PROCESSEDPEERREQUEST: {
            return api_.Storage().SetPeerRequestTime(
                nym.str(), request.str(), box);
        }
        default: {
            return false;
        }
    }
}

#if OT_CASH
std::unique_ptr<blind::Purse> Wallet::purse(
    const identifier::Nym& nym,
    const identifier::Server& server,
    const identifier::UnitDefinition& unit,
    const bool checking) const
{
    auto serialized = std::make_shared<proto::Purse>();
    const auto loaded =
        api_.Storage().Load(nym, server, unit, serialized, checking);

    if (false == loaded) {
        if (false == checking) {
            LogOutput(OT_METHOD)(__FUNCTION__)(": Purse does not exist")
                .Flush();
        }

        return {};
    }

    OT_ASSERT(serialized);

    if (false == proto::Validate(*serialized, VERBOSE)) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Invalid purse").Flush();

        return {};
    }

    std::unique_ptr<blind::Purse> output{
        opentxs::Factory::Purse(api_, *serialized)};

    if (false == bool(output)) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Failed to instantiate purse")
            .Flush();

        return {};
    }

    return output;
}

std::unique_ptr<const blind::Purse> Wallet::Purse(
    const identifier::Nym& nym,
    const identifier::Server& server,
    const identifier::UnitDefinition& unit,
    const bool checking) const
{
    return purse(nym, server, unit, checking);
}

Editor<blind::Purse> Wallet::mutable_Purse(
    const identifier::Nym& nymID,
    const identifier::Server& server,
    const identifier::UnitDefinition& unit,
    const PasswordPrompt& reason,
    const proto::CashType type) const
{
    auto pPurse = purse(nymID, server, unit, true);

    if (false == bool(pPurse)) {
        const auto nym = Nym(nymID);

        OT_ASSERT(nym);

        pPurse.reset(
            opentxs::Factory::Purse(api_, *nym, server, unit, type, reason));
    }

    OT_ASSERT(pPurse);

    const OTNymID otNymID{nymID};
    std::function<void(blind::Purse*, const Lock&)> callback =
        [=](blind::Purse* in, const Lock& lock) -> void {
        this->save(lock, otNymID, in);
    };

    return Editor<blind::Purse>(
        get_purse_lock(nymID, server, unit), pPurse.release(), callback);
}
#endif

bool Wallet::RemoveServer(const identifier::Server& id) const
{
    std::string server(id.str());
    Lock mapLock(server_map_lock_);
    auto deleted = server_map_.erase(server);

    if (0 != deleted) { return api_.Storage().RemoveServer(server); }

    return false;
}

bool Wallet::RemoveUnitDefinition(const identifier::UnitDefinition& id) const
{
    std::string unit(id.str());
    Lock mapLock(unit_map_lock_);
    auto deleted = unit_map_.erase(unit);

    if (0 != deleted) { return api_.Storage().RemoveUnitDefinition(unit); }

    return false;
}

void Wallet::publish_server(const identifier::Server& id) const
{
    server_publisher_->Send(id.str());
}

Wallet::UnitNameReverse Wallet::reverse_unit_map(const UnitNameMap& map)
{
    UnitNameReverse output{};

    for (const auto& [key, value] : map) { output.emplace(value, key); }

    return output;
}

void Wallet::save(
    const PasswordPrompt& reason,
    const std::string id,
    std::unique_ptr<opentxs::Account>& in,
    eLock&,
    bool success) const
{
    OT_ASSERT(in)

    auto& account = *in;
    const auto accountID = Identifier::Factory(id);

    if (false == success) {
        // Reload the last valid state for this Account.
        std::string serialized{""};
        std::string alias{""};
        const auto loaded = api_.Storage().Load(id, serialized, alias, false);

        OT_ASSERT(loaded)

        in.reset(account_factory(accountID, alias, serialized));

        OT_ASSERT(in);

        return;
    }

    const auto signerID = api_.Storage().AccountSigner(accountID);

    OT_ASSERT(false == signerID->empty())

    const auto signerNym = Nym(signerID);

    OT_ASSERT(signerNym)

    account.ReleaseSignatures();
    auto saved = account.SignContract(*signerNym, reason);

    OT_ASSERT(saved)

    saved = account.SaveContract();

    OT_ASSERT(saved)

    auto serialized = String::Factory();
    saved = in->SaveContractRaw(serialized);

    OT_ASSERT(saved)

    const auto contractID = api_.Storage().AccountContract(accountID);

    OT_ASSERT(false == contractID->empty())

    saved = api_.Storage().Store(
        accountID->str(),
        serialized->Get(),
        in->Alias(),
        api_.Storage().AccountOwner(accountID),
        api_.Storage().AccountSigner(accountID),
        api_.Storage().AccountIssuer(accountID),
        api_.Storage().AccountServer(accountID),
        contractID,
        extract_unit(contractID));

    OT_ASSERT(saved)
}

void Wallet::save(
    const PasswordPrompt& reason,
    opentxs::internal::Context* context) const
{
    if (nullptr == context) { return; }

    Lock lock(context->GetLock());
    const bool sig = context->UpdateSignature(lock, reason);

    OT_ASSERT(sig);
    OT_ASSERT(context->ValidateContext(lock));

    api_.Storage().Store(context->GetContract(lock));
}

void Wallet::save(const Lock& lock, api::client::Issuer* in) const
{
    OT_ASSERT(nullptr != in)
    OT_ASSERT(lock.owns_lock())

    const auto& nymID = in->LocalNymID();
    const auto& issuerID = in->IssuerID();
    api_.Storage().Store(nymID.str(), in->Serialize());
    auto message = issuer_publisher_->Context().Message(nymID.str());
    message->AddFrame(issuerID.str());
    issuer_publisher_->Send(message);
}

#if OT_CASH
void Wallet::save(const Lock& lock, const OTNymID nym, blind::Purse* in) const
{
    OT_ASSERT(nullptr != in)
    OT_ASSERT(lock.owns_lock())

    std::unique_ptr<blind::Purse> pPurse{in};

    auto& purse = *pPurse;
    const auto serialized = purse.Serialize();

    OT_ASSERT(proto::Validate(serialized, VERBOSE));

    const auto stored = api_.Storage().Store(nym, serialized);

    OT_ASSERT(stored);
}
#endif

void Wallet::save(NymData* nymData, const Lock& lock) const
{
    OT_ASSERT(nullptr != nymData);
    OT_ASSERT(lock.owns_lock())

    SaveCredentialIDs(nymData->nym());
}

void Wallet::save(
    const PasswordPrompt& reason,
    opentxs::NymFile* nymfile,
    const Lock& lock) const
{
    OT_ASSERT(nullptr != nymfile);
    OT_ASSERT(lock.owns_lock())

    auto* internal = dynamic_cast<opentxs::internal::NymFile*>(nymfile);

    OT_ASSERT(nullptr != internal)

    const auto saved = internal->SaveSignedNymFile(reason);

    OT_ASSERT(saved);
}

bool Wallet::SaveCredentialIDs(const identity::Nym& nym) const
{
    const auto index = dynamic_cast<const identity::internal::Nym&>(nym)
                           .SerializeCredentialIndex(
                               identity::internal::Nym::Mode::Abbreviated);
    const bool valid = proto::Validate(index, VERBOSE);

    if (!valid) { return false; }

    if (!api_.Storage().Store(index, nym.Alias())) {
        LogOutput(OT_METHOD)(__FUNCTION__)(
            ": Failure trying to store credential list for Nym: ")(nym.ID())
            .Flush();

        return false;
    }

    LogDetail(OT_METHOD)(__FUNCTION__)(": Credentials saved.").Flush();

    return true;
}

bool Wallet::SetNymAlias(const identifier::Nym& id, const std::string& alias)
    const
{
    Lock mapLock(nym_map_lock_);
    auto& nym = nym_map_[id.str()].second;

    nym->SetAlias(alias);

    return api_.Storage().SetNymAlias(id.str(), alias);
}

OTServerContract Wallet::Server(
    const identifier::Server& id,
    const std::chrono::milliseconds& timeout) const
{
    const std::string server = id.str();
    Lock mapLock(server_map_lock_);
    bool inMap = (server_map_.find(server) != server_map_.end());
    bool valid = false;

    if (!inMap) {
        std::shared_ptr<proto::ServerContract> serialized;

        std::string alias;
        bool loaded = api_.Storage().Load(server, serialized, alias, true);

        if (loaded) {
            auto nym = Nym(identifier::Nym::Factory(serialized->nymid()));

            if (!nym && serialized->has_publicnym()) {
                nym = Nym(serialized->publicnym());
            }

            if (nym) {
                auto& pServer = server_map_[server];
                pServer =
                    opentxs::Factory::ServerContract(api_, nym, *serialized);

                if (pServer) {
                    valid = true;  // Factory() performs validation
                    pServer->InitAlias(alias);
                } else {
                    server_map_.erase(server);
                }
            }
        } else {
            dht_server_requester_->Send(server);

            if (timeout > std::chrono::milliseconds(0)) {
                mapLock.unlock();
                auto start = std::chrono::high_resolution_clock::now();
                auto end = start + timeout;
                const auto interval = std::chrono::milliseconds(100);

                while (std::chrono::high_resolution_clock::now() < end) {
                    std::this_thread::sleep_for(interval);
                    mapLock.lock();
                    bool found =
                        (server_map_.find(server) != server_map_.end());
                    mapLock.unlock();

                    if (found) { break; }
                }

                return Server(id);  // timeout of zero prevents infinite
                                    // recursion
            }
        }
    } else {
        auto& pServer = server_map_[server];
        if (pServer) { valid = pServer->Validate(); }
    }

    if (valid) { return OTServerContract{server_map_[server]}; }

    throw std::runtime_error("Server contract not found");
}

OTServerContract Wallet::server(
    std::unique_ptr<contract::Server> contract) const noexcept(false)
{
    if (false == bool(contract)) {
        throw std::runtime_error("Null server contract");
    }

    if (false == contract->Validate()) {
        throw std::runtime_error("Invalid server contract");
    }

    const auto id =
        identifier::Server::Factory(contract->ID()->str());  // TODO conversion
    const auto server = id->str();
    const auto serverNymName = contract->EffectiveName();

    if (serverNymName != contract->Name()) {
        contract->SetAlias(serverNymName);
    }

    if (api_.Storage().Store(contract->Contract(), contract->Alias())) {
        Lock mapLock(server_map_lock_);
        server_map_[server].reset(contract.release());
        mapLock.unlock();
        publish_server(id);
    } else {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Failed to save server contract.")
            .Flush();
    }

    return Server(identifier::Server::Factory(server));
}

OTServerContract Wallet::Server(const proto::ServerContract& contract) const
{
    const auto& server = contract.id();
    auto serverID = identifier::Server::Factory(server);

    if (serverID->empty()) {
        throw std::runtime_error("Invalid server contract");
    }

    const auto nymID = identifier::Nym::Factory(contract.nymid());

    if (nymID->empty()) { throw std::runtime_error("Invalid nym ID"); }

    auto nym = Nym(nymID);

    if (false == bool(nym) && contract.has_publicnym()) {
        nym = Nym(contract.publicnym());
    }

    if (nym) {
        auto candidate = std::unique_ptr<contract::Server>{
            opentxs::Factory::ServerContract(api_, nym, contract)};

        if (candidate) {
            if (candidate->Validate()) {
                if (serverID.get() != candidate->ID()) {
                    LogOutput(OT_METHOD)(__FUNCTION__)(": Wrong contract ID.")
                        .Flush();
                    serverID->Assign(candidate->ID());
                }

                const auto stored = api_.Storage().Store(
                    candidate->Contract(), candidate->EffectiveName());

                if (stored) {
                    Lock mapLock(server_map_lock_);
                    server_map_[server].reset(candidate.release());
                    mapLock.unlock();
                    publish_server(serverID);
                }
            }
        }
    } else {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Invalid nym.").Flush();
    }

    return Server(serverID);
}

OTServerContract Wallet::Server(
    const std::string& nymid,
    const std::string& name,
    const std::string& terms,
    const std::list<contract::Server::Endpoint>& endpoints,
    const opentxs::PasswordPrompt& reason,
    const VersionNumber version) const
{
    std::string server;
    auto nym = Nym(identifier::Nym::Factory(nymid));

    if (nym) {
        auto list = std::list<Endpoint>{};
        std::transform(
            std::begin(endpoints),
            std::end(endpoints),
            std::back_inserter(list),
            [](const auto& in) -> Endpoint {
                return {static_cast<int>(std::get<0>(in)),
                        static_cast<int>(std::get<1>(in)),
                        std::get<2>(in),
                        std::get<3>(in),
                        std::get<4>(in)};
            });
        auto pContract =
            std::unique_ptr<contract::Server>{opentxs::Factory::ServerContract(
                api_, nym, list, terms, name, version, reason)};

        if (pContract) {

            return this->server(std::move(pContract));
        } else {
            LogOutput(OT_METHOD)(__FUNCTION__)(
                ": Error: Failed to create contract.")
                .Flush();
        }
    } else {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Error: Nym does not exist.")
            .Flush();
    }

    return Server(identifier::Server::Factory(server));
}

ObjectList Wallet::ServerList() const { return api_.Storage().ServerList(); }

OTNymID Wallet::server_to_nym(Identifier& input) const
{
    auto output = identifier::Nym::Factory();
    auto nym = Nym(identifier::Nym::Factory(input.str()));  // TODO conversion
    const bool inputIsNymID = bool(nym);

    if (inputIsNymID) {
        output->Assign(input);
        const auto list = ServerList();
        std::size_t matches = 0;

        for (const auto& item : list) {
            const auto& serverID = item.first;

            try {
                auto server = Server(identifier::Server::Factory(serverID));

                if (server->Nym()->ID() == input) {
                    matches++;
                    // set input to the notary ID
                    input.Assign(server->ID());
                }
            } catch (...) {
            }
        }

        OT_ASSERT(2 > matches);
    } else {
        try {
            const auto contract = Server(
                identifier::Server::Factory(input.str()));  // TODO conversion
            output->SetString(contract->Contract().nymid());
        } catch (...) {
            LogDetail(OT_METHOD)(__FUNCTION__)(": Non-existent server: ")(input)
                .Flush();
        }
    }

    return output;
}

bool Wallet::SetServerAlias(
    const identifier::Server& id,
    const std::string& alias) const
{
    const std::string server = id.str();
    const bool saved = api_.Storage().SetServerAlias(server, alias);

    if (saved) {
        Lock mapLock(server_map_lock_);
        server_map_.erase(server);
        publish_server(id);

        return true;
    }

    return false;
}

bool Wallet::SetUnitDefinitionAlias(
    const identifier::UnitDefinition& id,
    const std::string& alias) const
{
    const std::string unit = id.str();
    const bool saved = api_.Storage().SetUnitDefinitionAlias(unit, alias);

    if (saved) {
        Lock mapLock(unit_map_lock_);
        unit_map_.erase(unit);

        return true;
    }

    return false;
}

ObjectList Wallet::UnitDefinitionList() const
{
    return api_.Storage().UnitDefinitionList();
}

OTUnitDefinition Wallet::UnitDefinition(
    const identifier::UnitDefinition& id,
    const std::chrono::milliseconds& timeout) const
{
    const std::string unit = id.str();
    Lock mapLock(unit_map_lock_);
    bool inMap = (unit_map_.find(unit) != unit_map_.end());
    bool valid = false;

    if (!inMap) {
        std::shared_ptr<proto::UnitDefinition> serialized;

        std::string alias;
        bool loaded = api_.Storage().Load(unit, serialized, alias, true);

        if (loaded) {
            auto nym = Nym(identifier::Nym::Factory(serialized->nymid()));

            if (!nym && serialized->has_publicnym()) {
                nym = Nym(serialized->publicnym());
            }

            if (nym) {
                auto& pUnit = unit_map_[unit];
                pUnit =
                    opentxs::Factory::UnitDefinition(api_, nym, *serialized);

                if (pUnit) {
                    valid = true;  // Factory() performs validation
                    pUnit->InitAlias(alias);
                } else {
                    unit_map_.erase(unit);
                }
            }
        } else {
            dht_unit_requester_->Send(unit);

            if (timeout > std::chrono::milliseconds(0)) {
                mapLock.unlock();
                auto start = std::chrono::high_resolution_clock::now();
                auto end = start + timeout;
                const auto interval = std::chrono::milliseconds(100);

                while (std::chrono::high_resolution_clock::now() < end) {
                    std::this_thread::sleep_for(interval);
                    mapLock.lock();
                    bool found = (unit_map_.find(unit) != unit_map_.end());
                    mapLock.unlock();

                    if (found) { break; }
                }

                return UnitDefinition(id);  // timeout of zero prevents infinite
                                            // recursion
            }
        }
    } else {
        auto& pUnit = unit_map_[unit];
        if (pUnit) { valid = pUnit->Validate(); }
    }

    if (valid) { return OTUnitDefinition{unit_map_[unit]}; }

    throw std::runtime_error("Unit definition does not exist");
}

OTUnitDefinition Wallet::unit_definition(
    std::shared_ptr<contract::Unit>&& contract) const
{
    std::string unit = contract->ID()->str();

    if (contract) {
        if (contract->Validate()) {
            if (api_.Storage().Store(contract->Contract(), contract->Alias())) {
                Lock mapLock(unit_map_lock_);
                auto it = unit_map_.find(unit);

                if (unit_map_.end() == it) {
                    unit_map_.emplace(unit, std::move(contract));
                } else {
                    it->second = std::move(contract);
                }

                mapLock.unlock();
            }
        }
    }

    return UnitDefinition(identifier::UnitDefinition::Factory(unit));
}

OTUnitDefinition Wallet::UnitDefinition(
    const proto::UnitDefinition& contract) const
{
    const std::string unit = contract.id();
    const auto nymID = identifier::Nym::Factory(contract.nymid());
    find_nym_->Send(nymID->str());
    auto nym = Nym(nymID);

    if (!nym && contract.has_publicnym()) { nym = Nym(contract.publicnym()); }

    if (nym) {
        auto candidate = opentxs::Factory::UnitDefinition(api_, nym, contract);

        if (candidate) {
            if (candidate->Validate()) {
                if (api_.Storage().Store(
                        candidate->Contract(), candidate->Alias())) {
                    Lock mapLock(unit_map_lock_);
                    auto it = unit_map_.find(unit);

                    if (unit_map_.end() == it) {
                        unit_map_.emplace(unit, std::move(candidate));
                    } else {
                        it->second = std::move(candidate);
                    }

                    mapLock.unlock();
                }
            }
        }
    }

    return UnitDefinition(identifier::UnitDefinition::Factory(unit));
}

OTUnitDefinition Wallet::UnitDefinition(
    const std::string& nymid,
    const std::string& shortname,
    const std::string& name,
    const std::string& symbol,
    const std::string& terms,
    const std::string& tla,
    const std::uint32_t power,
    const std::string& fraction,
    const proto::ContactItemType unitOfAccount,
    const PasswordPrompt& reason,
    const VersionNumber version) const
{
    auto unit = std::string{};
    auto nym = Nym(identifier::Nym::Factory(nymid));

    if (nym) {
        auto contract = opentxs::Factory::CurrencyContract(
            api_,
            nym,
            shortname,
            name,
            symbol,
            terms,
            tla,
            power,
            fraction,
            unitOfAccount,
            version,
            reason);

        if (contract) {

            return unit_definition(std::move(contract));
        } else {
            LogOutput(OT_METHOD)(__FUNCTION__)(
                ": Error: Failed to create contract.")
                .Flush();
        }
    } else {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Error: Nym does not exist.")
            .Flush();
    }

    return UnitDefinition(identifier::UnitDefinition::Factory(unit));
}

OTUnitDefinition Wallet::UnitDefinition(
    const std::string& nymid,
    const std::string& shortname,
    const std::string& name,
    const std::string& symbol,
    const std::string& terms,
    const proto::ContactItemType unitOfAccount,
    const PasswordPrompt& reason,
    const VersionNumber version) const
{
    std::string unit;
    auto nym = Nym(identifier::Nym::Factory(nymid));

    if (nym) {
        auto contract = opentxs::Factory::SecurityContract(
            api_,
            nym,
            shortname,
            name,
            symbol,
            terms,
            unitOfAccount,
            version,
            reason);

        if (contract) {

            return unit_definition(std::move(contract));
        } else {
            LogOutput(OT_METHOD)(__FUNCTION__)(
                ": Error: Failed to create contract.")
                .Flush();
        }
    } else {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Error: Nym does not exist.")
            .Flush();
    }

    return UnitDefinition(identifier::UnitDefinition::Factory(unit));
}

bool Wallet::LoadCredential(
    const std::string& id,
    std::shared_ptr<proto::Credential>& credential) const
{
    return api_.Storage().Load(id, credential);
}

bool Wallet::SaveCredential(const proto::Credential& credential) const
{
    return api_.Storage().Store(credential);
}
}  // namespace opentxs::api::implementation
