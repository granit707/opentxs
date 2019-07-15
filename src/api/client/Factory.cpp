// Copyright (c) 2018 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "stdafx.hpp"

#include "Internal.hpp"

#include "opentxs/api/client/Contacts.hpp"
#include "opentxs/api/client/Manager.hpp"
#include "opentxs/api/crypto/Symmetric.hpp"
#include "opentxs/api/Factory.hpp"
#include "opentxs/core/contract/peer/PeerObject.hpp"
#include "opentxs/core/Log.hpp"

#include "internal/api/client/Client.hpp"
#include "internal/api/Api.hpp"
#include "api/Factory.hpp"

#include "Factory.hpp"

//#define OT_METHOD "opentxs::api::client::implementation::Factory::"

namespace opentxs
{
api::internal::Factory* Factory::FactoryAPIClient(
    const api::client::internal::Manager& api)
{
    return new api::client::implementation::Factory(api);
}
}  // namespace opentxs

namespace opentxs::api::client::implementation
{
Factory::Factory(const api::client::internal::Manager& client)
    : api::implementation::Factory(client)
    , client_(client)
{
}

std::unique_ptr<opentxs::PeerObject> Factory::PeerObject(
    const Nym_p& senderNym,
    const std::string& message,
    const opentxs::PasswordPrompt& reason) const
{
    return std::unique_ptr<opentxs::PeerObject>{
        opentxs::Factory::PeerObject(client_, senderNym, message, reason)};
}

std::unique_ptr<opentxs::PeerObject> Factory::PeerObject(
    const Nym_p& senderNym,
    const std::string& payment,
    const bool isPayment,
    const opentxs::PasswordPrompt& reason) const
{
    return std::unique_ptr<opentxs::PeerObject>{opentxs::Factory::PeerObject(
        client_, senderNym, payment, isPayment, reason)};
}

#if OT_CASH
std::unique_ptr<opentxs::PeerObject> Factory::PeerObject(
    const Nym_p& senderNym,
    const std::shared_ptr<blind::Purse> purse,
    const opentxs::PasswordPrompt& reason) const
{
    return std::unique_ptr<opentxs::PeerObject>{
        opentxs::Factory::PeerObject(client_, senderNym, purse, reason)};
}
#endif

std::unique_ptr<opentxs::PeerObject> Factory::PeerObject(
    const std::shared_ptr<const PeerRequest> request,
    const std::shared_ptr<const PeerReply> reply,
    const VersionNumber version,
    const opentxs::PasswordPrompt& reason) const
{
    return std::unique_ptr<opentxs::PeerObject>{
        opentxs::Factory::PeerObject(client_, request, reply, version, reason)};
}

std::unique_ptr<opentxs::PeerObject> Factory::PeerObject(
    const std::shared_ptr<const PeerRequest> request,
    const VersionNumber version,
    const opentxs::PasswordPrompt& reason) const
{
    return std::unique_ptr<opentxs::PeerObject>{
        opentxs::Factory::PeerObject(client_, request, version, reason)};
}

std::unique_ptr<opentxs::PeerObject> Factory::PeerObject(
    const Nym_p& signerNym,
    const proto::PeerObject& serialized,
    const opentxs::PasswordPrompt& reason) const
{
    return std::unique_ptr<opentxs::PeerObject>{opentxs::Factory::PeerObject(
        client_.Contacts(), client_, signerNym, serialized, reason)};
}

std::unique_ptr<opentxs::PeerObject> Factory::PeerObject(
    const Nym_p& recipientNym,
    const opentxs::Armored& encrypted,
    const opentxs::PasswordPrompt& reason) const
{
    return std::unique_ptr<opentxs::PeerObject>{opentxs::Factory::PeerObject(
        client_.Contacts(), client_, recipientNym, encrypted, reason)};
}
}  // namespace opentxs::api::client::implementation