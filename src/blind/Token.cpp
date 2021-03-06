// Copyright (c) 2010-2020 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "stdafx.hpp"
#include "Internal.hpp"

#if OT_CASH
#include "opentxs/blind/Purse.hpp"
#include "opentxs/core/Log.hpp"
#include "opentxs/crypto/key/Symmetric.hpp"

#if OT_CASH_USING_LUCRE
#include "internal/api/Api.hpp"
#include "token/Lucre.hpp"
#endif

#include "Token.hpp"

#define OT_METHOD "opentxs::blind::token::implementation::Token::"

namespace opentxs
{
using ReturnType = blind::token::implementation::Lucre;

auto Factory::Token(const blind::Token& token, blind::Purse& purse) noexcept
    -> std::unique_ptr<blind::Token>
{
    switch (token.Type()) {
        case proto::CASHTYPE_LUCRE: {

            return std::make_unique<ReturnType>(
                dynamic_cast<const ReturnType&>(token), purse);
        }
        default: {
            OT_FAIL;
        }
    }
}

auto Factory::Token(
    const api::internal::Core& api,
    blind::Purse& purse,
    const proto::Token& serialized) noexcept(false)
    -> std::unique_ptr<blind::Token>
{
    switch (serialized.type()) {
        case proto::CASHTYPE_LUCRE: {

            return std::make_unique<ReturnType>(api, purse, serialized);
        }
        default: {
            throw std::runtime_error("Unknown token type");
        }
    }
}

auto Factory::Token(
    const api::internal::Core& api,
    const identity::Nym& owner,
    const blind::Mint& mint,
    const blind::Token::Denomination value,
    blind::Purse& purse,
    const opentxs::PasswordPrompt& reason) -> std::unique_ptr<blind::Token>
{
    switch (purse.Type()) {
        case proto::CASHTYPE_LUCRE: {

            return std::make_unique<ReturnType>(
                api, owner, mint, value, purse, reason);
        }
        default: {
            throw std::runtime_error("Unknown token type");
        }
    }
}
}  // namespace opentxs

namespace opentxs::blind::token::implementation
{
const proto::SymmetricMode Token::mode_{proto::SMODE_CHACHA20POLY1305};

Token::Token(
    const api::internal::Core& api,
    Purse& purse,
    const proto::TokenState state,
    const proto::CashType type,
    const identifier::Server& notary,
    const identifier::UnitDefinition& unit,
    const std::uint64_t series,
    const Denomination denomination,
    const Time validFrom,
    const Time validTo,
    const VersionNumber version)
    : api_(api)
    , purse_(purse)
    , state_(state)
    , notary_(notary)
    , unit_(unit)
    , series_(series)
    , denomination_(denomination)
    , valid_from_(validFrom)
    , valid_to_(validTo)
    , type_(type)
    , version_(version)
{
}

Token::Token(const Token& rhs)
    : Token(
          rhs.api_,
          rhs.purse_,
          rhs.state_,
          rhs.type_,
          rhs.notary_,
          rhs.unit_,
          rhs.series_,
          rhs.denomination_,
          rhs.valid_from_,
          rhs.valid_to_,
          rhs.version_)
{
}

Token::Token(
    const api::internal::Core& api,
    Purse& purse,
    const proto::Token& in)
    : Token(
          api,
          purse,
          in.state(),
          in.type(),
          identifier::Server::Factory(in.notary()),
          identifier::UnitDefinition::Factory(in.mint()),
          in.series(),
          in.denomination(),
          Clock::from_time_t(in.validfrom()),
          Clock::from_time_t(in.validto()),
          in.version())
{
}

Token::Token(
    const api::internal::Core& api,
    Purse& purse,
    const VersionNumber version,
    const proto::TokenState state,
    const std::uint64_t series,
    const Denomination denomination,
    const Time validFrom,
    const Time validTo)
    : Token(
          api,
          purse,
          state,
          purse.Type(),
          purse.Notary(),
          purse.Unit(),
          series,
          denomination,
          validFrom,
          validTo,
          version)
{
}

bool Token::reencrypt(
    const crypto::key::Symmetric& oldKey,
    const PasswordPrompt& oldPassword,
    const crypto::key::Symmetric& newKey,
    const PasswordPrompt& newPassword,
    proto::Ciphertext& ciphertext)
{
    auto plaintext = Data::Factory();
    auto output =
        oldKey.Decrypt(ciphertext, oldPassword, plaintext->WriteInto());

    if (false == output) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Failed to decrypt ciphertext.")
            .Flush();

        return false;
    }

    output = newKey.Encrypt(
        plaintext->Bytes(),
        newPassword,
        ciphertext,
        false,
        proto::SMODE_CHACHA20POLY1305);

    if (false == output) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Failed to encrypt ciphertext.")
            .Flush();

        return false;
    }

    return output;
}

proto::Token Token::Serialize() const
{
    proto::Token output{};
    output.set_version(version_);
    output.set_type(type_);
    output.set_state(state_);
    output.set_notary(notary_->str());
    output.set_mint(unit_->str());
    output.set_series(series_);
    output.set_denomination(denomination_);
    output.set_validfrom(Clock::to_time_t(valid_from_));
    output.set_validto(Clock::to_time_t(valid_to_));

    return output;
}
}  // namespace opentxs::blind::token::implementation
#endif
