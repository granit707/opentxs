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

#ifndef OPENTXS_UI_PROFILE_SECTION_PARENT_HPP
#define OPENTXS_UI_PROFILE_SECTION_PARENT_HPP

#include "Internal.hpp"

#include <string>

namespace opentxs::ui::implementation
{
class ProfileSectionParent
{
public:
    using ProfileSectionIDType =
        std::pair<proto::ContactSectionName, proto::ContactItemType>;
    using ProfileSectionSortKey = int;

    EXPORT virtual bool AddClaim(
        const proto::ContactItemType type,
        const std::string& value,
        const bool primary,
        const bool active) const = 0;
    virtual bool last(const ProfileSectionIDType& id) const = 0;
    virtual const Identifier& NymID() const = 0;
    virtual void reindex_item(
        const ProfileSectionIDType& id,
        const ProfileSectionSortKey& newIndex) const = 0;
    virtual proto::ContactSectionName Type() const = 0;
    virtual OTIdentifier WidgetID() const = 0;

    virtual ~ProfileSectionParent() = default;

protected:
    ProfileSectionParent() = default;
    ProfileSectionParent(const ProfileSectionParent&) = delete;
    ProfileSectionParent(ProfileSectionParent&&) = delete;
    ProfileSectionParent& operator=(const ProfileSectionParent&) = delete;
    ProfileSectionParent& operator=(ProfileSectionParent&&) = delete;
};
}  // namespace opentxs::ui::implementation
#endif  // OPENTXS_UI_PROFILE_SECTION_PARENT_HPP
