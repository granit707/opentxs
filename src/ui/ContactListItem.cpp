// Copyright (c) 2010-2020 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "stdafx.hpp"

#include "opentxs/api/client/Contacts.hpp"
#include "opentxs/core/Identifier.hpp"
#include "opentxs/core/Lockable.hpp"
#include "opentxs/network/zeromq/socket/Subscribe.hpp"
#include "opentxs/network/zeromq/Context.hpp"
#include "opentxs/network/zeromq/ListenCallback.hpp"
#include "opentxs/network/zeromq/FrameIterator.hpp"
#include "opentxs/network/zeromq/FrameSection.hpp"
#include "opentxs/network/zeromq/Frame.hpp"
#include "opentxs/network/zeromq/Message.hpp"
#include "opentxs/ui/ContactList.hpp"
#include "opentxs/ui/ContactListItem.hpp"

#include <locale>

#include "internal/ui/UI.hpp"
#include "Row.hpp"

#include "ContactListItem.hpp"

template class opentxs::SharedPimpl<opentxs::ui::ContactListItem>;

//#define OT_METHOD "opentxs::ui::implementation::ContactListItem::"

namespace opentxs
{
ui::internal::ContactListItem* Factory::ContactListItem(
    const ui::implementation::ContactListInternalInterface& parent,
    const api::client::internal::Manager& api,
    const network::zeromq::socket::Publish& publisher,
    const ui::implementation::ContactListRowID& rowID,
    const ui::implementation::ContactListSortKey& key)
{
    return new ui::implementation::ContactListItem(
        parent, api, publisher, rowID, key);
}
}  // namespace opentxs

namespace opentxs::ui::implementation
{
ContactListItem::ContactListItem(
    const ContactListInternalInterface& parent,
    const api::client::internal::Manager& api,
    const network::zeromq::socket::Publish& publisher,
    const ContactListRowID& rowID,
    const ContactListSortKey& key) noexcept
    : ContactListItemRow(parent, api, publisher, rowID, true)
    , key_(key)
{
}

std::string ContactListItem::ContactID() const noexcept
{
    return row_id_->str();
}

std::string ContactListItem::DisplayName() const noexcept
{
    Lock lock(lock_);

    return key_;
}

std::string ContactListItem::ImageURI() const noexcept
{
    // TODO

    return {};
}

#if OT_QT
QVariant ContactListItem::qt_data([[maybe_unused]] const int column, int role)
    const noexcept
{
    switch (role) {
        case Qt::DisplayRole: {
            return DisplayName().c_str();
        }
        case Qt::DecorationRole: {
            // TODO render ImageURI into a QPixmap
            return {};
        }
        case ContactListQt::ContactIDRole: {
            return ContactID().c_str();
        }
        case ContactListQt::SectionRole: {
            return Section().c_str();
        }
        default: {
            return {};
        }
    }
}
#endif

void ContactListItem::reindex(
    const ContactListSortKey& key,
    const CustomData&) noexcept
{
    Lock lock(lock_);
    key_ = key;
}

std::string ContactListItem::Section() const noexcept
{
    Lock lock(lock_);

    if (row_id_ == parent_.ID()) { return {"ME"}; }

    if (key_.empty()) { return {" "}; }

    std::locale locale;
    std::string output{" "};
    output[0] = std::toupper(key_[0], locale);

    return output;
}
}  // namespace opentxs::ui::implementation
