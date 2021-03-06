// Copyright (c) 2010-2020 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

namespace opentxs::contract::peer::reply::implementation
{
class Acknowledgement final : public reply::Acknowledgement,
                              public peer::implementation::Reply
{
public:
    Acknowledgement(
        const api::internal::Core& api,
        const Nym_p& nym,
        const identifier::Nym& initiator,
        const Identifier& request,
        const identifier::Server& server,
        const proto::PeerRequestType type,
        const bool& ack);
    Acknowledgement(
        const api::internal::Core& api,
        const Nym_p& nym,
        const SerializedType& serialized);

    ~Acknowledgement() final = default;

private:
    friend opentxs::Factory;

    const bool ack_;

    Acknowledgement* clone() const noexcept final
    {
        return new Acknowledgement(*this);
    }
    SerializedType IDVersion(const Lock& lock) const final;

    Acknowledgement() = delete;
    Acknowledgement(const Acknowledgement&);
    Acknowledgement(Acknowledgement&&) = delete;
    Acknowledgement& operator=(const Acknowledgement&) = delete;
    Acknowledgement& operator=(Acknowledgement&&) = delete;
};
}  // namespace opentxs::contract::peer::reply::implementation
