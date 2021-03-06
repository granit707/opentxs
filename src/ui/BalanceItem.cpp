// Copyright (c) 2010-2020 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "stdafx.hpp"

#include "opentxs/api/client/Contacts.hpp"
#include "opentxs/api/client/Manager.hpp"
#include "opentxs/api/client/OTX.hpp"
#include "opentxs/api/client/Workflow.hpp"
#include "opentxs/api/storage/Storage.hpp"
#include "opentxs/api/Core.hpp"
#include "opentxs/api/Factory.hpp"
#include "opentxs/api/Wallet.hpp"
#include "opentxs/core/contract/UnitDefinition.hpp"
#include "opentxs/core/Cheque.hpp"
#include "opentxs/core/Flag.hpp"
#include "opentxs/core/Identifier.hpp"
#include "opentxs/core/Lockable.hpp"
#include "opentxs/core/Log.hpp"
#include "opentxs/core/PasswordPrompt.hpp"
#include "opentxs/ui/BalanceItem.hpp"

#include "internal/api/client/Client.hpp"
#include "internal/ui/UI.hpp"
#include "Row.hpp"

#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include "BalanceItem.hpp"
#include "ChequeBalanceItem.hpp"
#include "TransferBalanceItem.hpp"

template class opentxs::SharedPimpl<opentxs::ui::BalanceItem>;

#define OT_METHOD "opentxs::ui::implementation::BalanceItem::"

namespace opentxs
{
ui::implementation::AccountActivityRowInternal* Factory::BalanceItem(
    const ui::implementation::AccountActivityInternalInterface& parent,
    const api::client::internal::Manager& api,
    const network::zeromq::socket::Publish& publisher,
    const ui::implementation::AccountActivityRowID& rowID,
    const ui::implementation::AccountActivitySortKey& sortKey,
    const ui::implementation::CustomData& custom,
    const identifier::Nym& nymID,
    const Identifier& accountID)
{
    const auto type =
        ui::implementation::BalanceItem::recover_workflow(custom).type();

    switch (type) {
        case proto::PAYMENTWORKFLOWTYPE_OUTGOINGCHEQUE:
        case proto::PAYMENTWORKFLOWTYPE_INCOMINGCHEQUE:
        case proto::PAYMENTWORKFLOWTYPE_OUTGOINGINVOICE:
        case proto::PAYMENTWORKFLOWTYPE_INCOMINGINVOICE: {
            return new ui::implementation::ChequeBalanceItem(
                parent,
                api,
                publisher,
                rowID,
                sortKey,
                custom,
                nymID,
                accountID);
        }
        case proto::PAYMENTWORKFLOWTYPE_OUTGOINGTRANSFER:
        case proto::PAYMENTWORKFLOWTYPE_INCOMINGTRANSFER:
        case proto::PAYMENTWORKFLOWTYPE_INTERNALTRANSFER: {
            return new ui::implementation::TransferBalanceItem(
                parent,
                api,
                publisher,
                rowID,
                sortKey,
                custom,
                nymID,
                accountID);
        }
        case proto::PAYMENTWORKFLOWTYPE_ERROR:
        default: {
            LogOutput(OT_METHOD)(__FUNCTION__)(": Unhandled workflow type (")(
                type)(")")
                .Flush();
        }
    }

    return nullptr;
}
}  // namespace opentxs

namespace opentxs::ui::implementation
{
BalanceItem::BalanceItem(
    const AccountActivityInternalInterface& parent,
    const api::client::internal::Manager& api,
    const network::zeromq::socket::Publish& publisher,
    const AccountActivityRowID& rowID,
    const AccountActivitySortKey& sortKey,
    const CustomData& custom,
    const identifier::Nym& nymID,
    const Identifier& accountID) noexcept
    : BalanceItemRow(parent, api, publisher, rowID, true)
    , nym_id_(nymID)
    , workflow_(recover_workflow(custom).id())
    , type_(extract_type(recover_workflow(custom)))
    , text_("")
    , time_(sortKey)
    , contract_(api.Factory().UnitDefinition())
    , startup_(nullptr)
    , account_id_(Identifier::Factory(accountID))
    , contacts_(extract_contacts(api_, recover_workflow(custom)))
{
}

std::string BalanceItem::DisplayAmount() const noexcept
{
    sLock lock(shared_lock_);

    if (get_contract()) {
        const auto amount = effective_amount();
        std::string output{};
        const auto formatted =
            contract_->FormatAmountLocale(amount, output, ",", ".");

        if (formatted) { return output; }

        return std::to_string(amount);
    }

    return std::to_string(0);
}

std::vector<std::string> BalanceItem::extract_contacts(
    const api::client::internal::Manager& api,
    const proto::PaymentWorkflow& workflow) noexcept
{
    std::vector<std::string> output{};

    for (const auto& party : workflow.party()) {
        const auto contactID =
            api.Contacts().NymToContact(identifier::Nym::Factory(party));
        output.emplace_back(contactID->str());
    }

    return output;
}

StorageBox BalanceItem::extract_type(
    const proto::PaymentWorkflow& workflow) noexcept
{
    switch (workflow.type()) {
        case proto::PAYMENTWORKFLOWTYPE_OUTGOINGCHEQUE: {

            return StorageBox::OUTGOINGCHEQUE;
        }
        case proto::PAYMENTWORKFLOWTYPE_INCOMINGCHEQUE: {

            return StorageBox::INCOMINGCHEQUE;
        }
        case proto::PAYMENTWORKFLOWTYPE_OUTGOINGTRANSFER: {

            return StorageBox::OUTGOINGTRANSFER;
        }
        case proto::PAYMENTWORKFLOWTYPE_INCOMINGTRANSFER: {

            return StorageBox::INCOMINGTRANSFER;
        }
        case proto::PAYMENTWORKFLOWTYPE_INTERNALTRANSFER: {

            return StorageBox::INTERNALTRANSFER;
        }
        case proto::PAYMENTWORKFLOWTYPE_ERROR:
        case proto::PAYMENTWORKFLOWTYPE_OUTGOINGINVOICE:
        case proto::PAYMENTWORKFLOWTYPE_INCOMINGINVOICE:
        default: {

            return StorageBox::UNKNOWN;
        }
    }
}

std::string BalanceItem::get_contact_name(const identifier::Nym& nymID) const
    noexcept
{
    if (nymID.empty()) { return {}; }

    std::string output{nymID.str()};
    const auto contactID = api_.Contacts().ContactID(nymID);

    if (false == contactID->empty()) {
        output = api_.Contacts().ContactName(contactID);
    }

    return output;
}

#if OT_QT
QVariant BalanceItem::qt_data(const int column, int role) const noexcept
{
    switch (role) {
        case Qt::DisplayRole: {
            switch (column) {
                case AccountActivityQt::AmountColumn: {
                    return DisplayAmount().c_str();
                }
                case AccountActivityQt::TextColumn: {
                    return Text().c_str();
                }
                case AccountActivityQt::MemoColumn: {
                    return Memo().c_str();
                }
                case AccountActivityQt::TimeColumn: {
                    auto qdatetime = QDateTime{};
                    qdatetime.setSecsSinceEpoch(
                        std::chrono::system_clock::to_time_t(Timestamp()));

                    return qdatetime;
                }
                case AccountActivityQt::UUIDColumn: {
                    return UUID().c_str();
                }
                default: {
                    return {};
                }
            }
        }
        case AccountActivityQt::PolarityRole: {
            return polarity(Amount());
        }
        case AccountActivityQt::ContactsRole: {
            std::string contacts;
            auto contact = Contacts().cbegin();

            if (contact != Contacts().cend()) {
                contacts = *contact;
                while (++contact != Contacts().cend()) {
                    contacts += ", " + *contact;
                }
            }

            return contacts.c_str();
        }
        case AccountActivityQt::WorkflowRole: {
            return Workflow().c_str();
        }
        case AccountActivityQt::TypeRole: {
            return static_cast<int>(Type());
        }
        default: {
            return {};
        }
    };
}
#endif

const proto::PaymentEvent& BalanceItem::recover_event(
    const CustomData& custom) noexcept
{
    OT_ASSERT(2 == custom.size())

    const auto& input = custom.at(1);

    OT_ASSERT(nullptr != input)

    return *static_cast<const proto::PaymentEvent*>(input);
}

const proto::PaymentWorkflow& BalanceItem::recover_workflow(
    const CustomData& custom) noexcept
{
    OT_ASSERT(2 == custom.size())

    const auto& input = custom.at(0);

    OT_ASSERT(nullptr != input)

    return *static_cast<const proto::PaymentWorkflow*>(input);
}

void BalanceItem::reindex(
    const implementation::AccountActivitySortKey& key,
    const implementation::CustomData&) noexcept
{
    eLock lock(shared_lock_);
    time_ = key;
    lock.unlock();
}

std::string BalanceItem::Text() const noexcept
{
    sLock lock(shared_lock_);

    return text_;
}

std::chrono::system_clock::time_point BalanceItem::Timestamp() const noexcept
{
    sLock lock(shared_lock_);

    return time_;
}

BalanceItem::~BalanceItem()
{
    if (startup_ && startup_->joinable()) {
        startup_->join();
        startup_.reset();
    }
}
}  // namespace opentxs::ui::implementation
