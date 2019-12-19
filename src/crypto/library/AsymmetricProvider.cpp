// Copyright (c) 2010-2019 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "stdafx.hpp"

#include "opentxs/core/crypto/OTPassword.hpp"
#include "opentxs/core/crypto/Signature.hpp"
#include "opentxs/core/Data.hpp"
#include "opentxs/core/Log.hpp"
#include "opentxs/core/String.hpp"

#include "util/Sodium.hpp"

extern "C" {
#include <sodium.h>
}

#include "AsymmetricProvider.hpp"

#define OT_METHOD "opentxs::crypto::AsymmetricProvider::"

namespace opentxs::crypto
{
proto::AsymmetricKeyType AsymmetricProvider::CurveToKeyType(
    const EcdsaCurve& curve)
{
    proto::AsymmetricKeyType output = proto::AKEYTYPE_ERROR;

    switch (curve) {
        case (EcdsaCurve::secp256k1): {
            output = proto::AKEYTYPE_SECP256K1;

            break;
        }
        case (EcdsaCurve::ed25519): {
            output = proto::AKEYTYPE_ED25519;

            break;
        }
        default: {
        }
    }

    return output;
}

EcdsaCurve AsymmetricProvider::KeyTypeToCurve(
    const proto::AsymmetricKeyType& type)
{
    EcdsaCurve output = EcdsaCurve::invalid;

    switch (type) {
        case (proto::AKEYTYPE_SECP256K1): {
            output = EcdsaCurve::secp256k1;

            break;
        }
        case (proto::AKEYTYPE_ED25519): {
            output = EcdsaCurve::ed25519;

            break;
        }
        default: {
        }
    }

    return output;
}
}  // namespace opentxs::crypto

namespace opentxs::crypto::implementation
{
AsymmetricProvider::AsymmetricProvider() noexcept
{
    if (0 > ::sodium_init()) { OT_FAIL; }
}

auto AsymmetricProvider::SeedToCurveKey(
    const ReadView seed,
    const AllocateOutput privateKey,
    const AllocateOutput publicKey) const noexcept -> bool
{
    auto edPublic = Data::Factory();
    auto edPrivate = OTPassword{};

    if (false == sodium::ExpandSeed(
                     seed,
                     edPrivate.WriteInto(OTPassword::Mode::Mem),
                     edPublic->WriteInto())) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Failed to expand seed.").Flush();

        return false;
    }

    if (false == bool(privateKey) || false == bool(publicKey)) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Invalid output allocator")
            .Flush();

        return false;
    }

    return sodium::ToCurveKeypair(
        edPrivate.Bytes(), edPublic->Bytes(), privateKey, publicKey);
}

auto AsymmetricProvider::SignContract(
    const api::internal::Core& api,
    const String& strContractUnsigned,
    const key::Asymmetric& theKey,
    Signature& theSignature,  // output
    const proto::HashType hashType,
    const PasswordPrompt& reason) const -> bool
{
    auto plaintext = Data::Factory(
        strContractUnsigned.Get(),
        strContractUnsigned.GetLength() + 1);  // include null terminator
    auto signature = Data::Factory();
    bool success = Sign(api, plaintext, theKey, hashType, signature, reason);
    theSignature.SetData(signature, true);  // true means, "yes, with newlines
                                            // in the b64-encoded output,
                                            // please."

    if (false == success) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Failed to sign contract").Flush();
    }

    return success;
}

auto AsymmetricProvider::VerifyContractSignature(
    const String& strContractToVerify,
    const key::Asymmetric& theKey,
    const Signature& theSignature,
    const proto::HashType hashType) const -> bool
{
    auto plaintext = Data::Factory(
        strContractToVerify.Get(),
        strContractToVerify.GetLength() + 1);  // include null terminator
    auto signature = Data::Factory();
    theSignature.GetData(signature);

    return Verify(plaintext, theKey, signature, hashType);
}
}  // namespace opentxs::crypto::implementation
