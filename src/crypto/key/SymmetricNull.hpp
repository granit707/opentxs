// Copyright (c) 2010-2020 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "Internal.hpp"

namespace opentxs::crypto::key::implementation
{
class SymmetricNull final : virtual public key::Symmetric
{
public:
    const api::internal::Core& api() const final { throw; }

    bool ChangePassword(const PasswordPrompt&, const OTPassword&) final
    {
        return false;
    }
    bool Decrypt(
        const proto::Ciphertext&,
        const PasswordPrompt&,
        const AllocateOutput) const final
    {
        return false;
    }
    bool Encrypt(
        const ReadView,
        const PasswordPrompt&,
        proto::Ciphertext&,
        const bool,
        const proto::SymmetricMode,
        const ReadView) const final
    {
        return false;
    }
    OTIdentifier ID(const PasswordPrompt&) const final
    {
        return Identifier::Factory();
    }
    bool RawKey(const PasswordPrompt&, OTPassword&) const final
    {
        return false;
    }
    bool Serialize(proto::SymmetricKey&) const final { return false; }
    bool Unlock(const PasswordPrompt&) const final { return false; }

    operator bool() const final { return false; }

    SymmetricNull() = default;
    ~SymmetricNull() = default;

private:
    SymmetricNull* clone() const final { return nullptr; }

    SymmetricNull(const SymmetricNull&) = delete;
    SymmetricNull(SymmetricNull&&) = delete;
    SymmetricNull& operator=(const SymmetricNull&) = delete;
    SymmetricNull& operator=(SymmetricNull&&) = delete;
};
}  // namespace opentxs::crypto::key::implementation
