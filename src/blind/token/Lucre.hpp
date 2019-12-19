// Copyright (c) 2010-2019 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "opentxs/core/Data.hpp"

#include "blind/Token.hpp"

#include <memory>
#include <stdexcept>

namespace opentxs::blind::token::implementation
{
class Lucre final : public Token
{
public:
    bool GetSpendable(String& output, const PasswordPrompt& reason) const;
    std::string ID(const PasswordPrompt& reason) const final;
    bool IsSpent(const PasswordPrompt& reason) const final;
    proto::Token Serialize() const final;

    bool AddSignature(const String& signature);
    bool ChangeOwner(
        Purse& oldOwner,
        Purse& newOwner,
        const PasswordPrompt& reason) final;
    bool GenerateTokenRequest(
        const identity::Nym& owner,
        const Mint& mint,
        const PasswordPrompt& reason) final;
    bool GetPublicPrototoken(String& output, const PasswordPrompt& reason);
    bool MarkSpent(const PasswordPrompt& reason) final;
    bool Process(
        const identity::Nym& owner,
        const Mint& mint,
        const PasswordPrompt& reason) final;

    Lucre(
        const api::internal::Core& api,
        Purse& purse,
        const proto::Token& serialized);
    Lucre(
        const api::internal::Core& api,
        const identity::Nym& owner,
        const Mint& mint,
        const Denomination value,
        Purse& purse,
        const PasswordPrompt& reason);
    Lucre(const Lucre& rhs, blind::Purse& newOwner);

    ~Lucre() final = default;

private:
    friend opentxs::Factory;

    const VersionNumber lucre_version_;
    OTString signature_;
    std::shared_ptr<proto::Ciphertext> private_;
    std::shared_ptr<proto::Ciphertext> public_;
    std::shared_ptr<proto::Ciphertext> spend_;

    void serialize_private(proto::LucreTokenData& lucre) const;
    void serialize_public(proto::LucreTokenData& lucre) const;
    void serialize_signature(proto::LucreTokenData& lucre) const;
    void serialize_spendable(proto::LucreTokenData& lucre) const;

    Lucre* clone() const noexcept final { return new Lucre(*this); }

    Lucre(
        const api::internal::Core& api,
        Purse& purse,
        const VersionNumber version,
        const proto::TokenState state,
        const std::uint64_t series,
        const Denomination value,
        const Time validFrom,
        const Time validTo,
        const String& signature,
        const std::shared_ptr<proto::Ciphertext> publicKey,
        const std::shared_ptr<proto::Ciphertext> privateKey,
        const std::shared_ptr<proto::Ciphertext> spendable);
    Lucre() = delete;
    Lucre(const Lucre&);
    Lucre(Lucre&&) = delete;
    Lucre& operator=(const Lucre&) = delete;
    Lucre& operator=(Lucre&&) = delete;
};
}  // namespace opentxs::blind::token::implementation
