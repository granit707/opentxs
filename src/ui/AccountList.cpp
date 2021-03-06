// Copyright (c) 2010-2020 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "stdafx.hpp"

#include "opentxs/api/client/Activity.hpp"
#include "opentxs/api/client/Contacts.hpp"
#include "opentxs/api/client/Manager.hpp"
#include "opentxs/api/storage/Storage.hpp"
#include "opentxs/api/Endpoints.hpp"
#include "opentxs/api/Factory.hpp"
#include "opentxs/api/Wallet.hpp"
#include "opentxs/core/crypto/OTPassword.hpp"
#include "opentxs/core/identifier/Nym.hpp"
#include "opentxs/core/identifier/Server.hpp"
#include "opentxs/core/identifier/UnitDefinition.hpp"
#include "opentxs/core/Identifier.hpp"
#include "opentxs/core/Log.hpp"
#include "opentxs/core/PasswordPrompt.hpp"
#include "opentxs/network/zeromq/socket/Subscribe.hpp"
#include "opentxs/network/zeromq/Context.hpp"
#include "opentxs/network/zeromq/ListenCallback.hpp"
#include "opentxs/network/zeromq/FrameIterator.hpp"
#include "opentxs/network/zeromq/FrameSection.hpp"
#include "opentxs/network/zeromq/Frame.hpp"
#include "opentxs/network/zeromq/Message.hpp"
#include "opentxs/ui/AccountListItem.hpp"

#include "internal/api/client/Client.hpp"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <thread>
#include <tuple>
#include <vector>

#include "AccountList.hpp"

#define OT_METHOD "opentxs::ui::implementation::AccountList::"

namespace opentxs
{
ui::implementation::AccountList* Factory::AccountListModel(
    const api::client::internal::Manager& api,
    const network::zeromq::socket::Publish& publisher,
    const identifier::Nym& nymID
#if OT_QT
    ,
    const bool qt
#endif
)
{
    return new ui::implementation::AccountList(
        api,
        publisher,
        nymID
#if OT_QT
        ,
        qt
#endif
    );
}

#if OT_QT
ui::AccountListQt* Factory::AccountListQtModel(
    ui::implementation::AccountList& parent)
{
    using ReturnType = ui::AccountListQt;

    return new ReturnType(parent);
}
#endif  // OT_QT
}  // namespace opentxs

#if OT_QT
namespace opentxs::ui
{
QT_PROXY_MODEL_WRAPPER(AccountListQt, implementation::AccountList)
}  // namespace opentxs::ui
#endif

namespace opentxs::ui::implementation
{
AccountList::AccountList(
    const api::client::internal::Manager& api,
    const network::zeromq::socket::Publish& publisher,
    const identifier::Nym& nymID
#if OT_QT
    ,
    const bool qt
#endif
    ) noexcept
    : AccountListList(
          api,
          publisher,
          nymID
#if OT_QT
          ,
          qt,
          Roles{
              {AccountListQt::NotaryIDRole, "notary"},
              {AccountListQt::UnitRole, "unit"},
              {AccountListQt::AccountIDRole, "account"},
              {AccountListQt::BalanceRole, "balance"},
              {AccountListQt::PolarityRole, "polarity"},
              {AccountListQt::AccountTypeRole, "accounttype"},
              {AccountListQt::ContractIdRole, "contractid"},
          },
          4
#endif
          )
    , listeners_{
          {api_.Endpoints().AccountUpdate(),
           new MessageProcessor<AccountList>(&AccountList::process_account)}}
{
    init();
    setup_listeners(listeners_);
    startup_.reset(new std::thread(&AccountList::startup, this));

    OT_ASSERT(startup_)
}

void* AccountList::construct_row(
    const AccountListRowID& id,
    const AccountListSortKey& index,
    const CustomData& custom) const noexcept
{
    names_.emplace(id, index);
    const auto [it, added] = items_[index].emplace(
        id,
        Factory::AccountListItem(*this, api_, publisher_, id, index, custom));

    return it->second.get();
}

void AccountList::process_account(const Identifier& id) noexcept
{
    auto account = api_.Wallet().Account(id);
    process_account(id, account.get().GetBalance(), account.get().Alias());
}

void AccountList::process_account(
    const Identifier& id,
    const Amount balance) noexcept
{
    auto account = api_.Wallet().Account(id);
    process_account(id, balance, account.get().Alias());
}

void AccountList::process_account(
    const Identifier& id,
    const Amount balance,
    const std::string& name) noexcept
{
    auto index = make_blank<AccountListSortKey>::value(api_);
    auto& [type, notary] = index;
    type = api_.Storage().AccountUnit(id);
    notary = api_.Storage().AccountServer(id)->str();
    const auto contract = api_.Storage().AccountContract(id);
    CustomData custom{};
    custom.emplace_back(new Amount{balance});
    custom.emplace_back(new OTUnitID{contract});
    custom.emplace_back(new std::string{name});
    add_item(id, index, custom);
}

void AccountList::process_account(
    const network::zeromq::Message& message) noexcept
{
    wait_for_startup();

    OT_ASSERT(2 == message.Body().size());

    const std::string id(message.Body_at(0));
    const auto& balanceFrame = message.Body_at(1);
    const auto accountID = Identifier::Factory(id);
    Amount balance{0};
    OTPassword::safe_memcpy(
        &balance, sizeof(balance), balanceFrame.data(), balanceFrame.size());

    OT_ASSERT(false == accountID->empty())

    if (0 < names_.count(accountID)) {
        Lock lock(lock_);
        delete_item(lock, accountID);
    }

    OT_ASSERT(0 == names_.count(accountID));

    process_account(accountID, balance);
}

void AccountList::startup() noexcept
{
    const auto accounts = api_.Storage().AccountsByOwner(primary_id_);
    LogDetail(OT_METHOD)(__FUNCTION__)(": Loading ")(accounts.size())(
        " accounts.")
        .Flush();

    for (const auto& id : accounts) { process_account(id); }

    finish_startup();
}

AccountList::~AccountList()
{
    for (auto& it : listeners_) { delete it.second; }
}
}  // namespace opentxs::ui::implementation
