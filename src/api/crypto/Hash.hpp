// Copyright (c) 2010-2020 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

namespace opentxs::api::crypto::implementation
{
class Hash final : public api::crypto::Hash
{
public:
    bool Digest(
        const proto::HashType hashType,
        const ReadView data,
        const AllocateOutput destination) const noexcept final;
    bool Digest(
        const proto::HashType hashType,
        const opentxs::network::zeromq::Frame& data,
        const AllocateOutput destination) const noexcept final;
    bool Digest(
        const std::uint32_t type,
        const ReadView data,
        const AllocateOutput destination) const noexcept final;
    bool HMAC(
        const proto::HashType hashType,
        const ReadView key,
        const ReadView& data,
        const AllocateOutput digest) const noexcept final;
    void MurmurHash3_32(
        const std::uint32_t& key,
        const Data& data,
        std::uint32_t& output) const noexcept final;
    bool PKCS5_PBKDF2_HMAC(
        const Data& input,
        const Data& salt,
        const std::size_t iterations,
        const proto::HashType hashType,
        const std::size_t bytes,
        Data& output) const noexcept final;
    bool PKCS5_PBKDF2_HMAC(
        const OTPassword& input,
        const Data& salt,
        const std::size_t iterations,
        const proto::HashType hashType,
        const std::size_t bytes,
        Data& output) const noexcept final;
    bool PKCS5_PBKDF2_HMAC(
        const std::string& input,
        const Data& salt,
        const std::size_t iterations,
        const proto::HashType hashType,
        const std::size_t bytes,
        Data& output) const noexcept final;
    bool SipHash(
        const OTPassword& key,
        const Data& data,
        std::uint64_t& output,
        const int c,
        const int d) const noexcept final;

    Hash(
        const api::crypto::Encode& encode,
        const opentxs::crypto::HashingProvider& ssl,
        const opentxs::crypto::HashingProvider& sodium,
        const opentxs::crypto::Pbkdf2& pbkdf2,
        const opentxs::crypto::Ripemd160& ripe) noexcept;

    ~Hash() = default;

private:
    friend opentxs::Factory;

    const api::crypto::Encode& encode_;
    const opentxs::crypto::HashingProvider& ssl_;
    const opentxs::crypto::HashingProvider& sodium_;
    const opentxs::crypto::Pbkdf2& pbkdf2_;
    const opentxs::crypto::Ripemd160& ripe_;

    static auto allocate(
        const proto::HashType hashType,
        const AllocateOutput destination) noexcept -> WritableView;

    auto bitcoin_hash_160(
        const void* input,
        const std::size_t size,
        void* output) const noexcept -> bool;
    bool digest(
        const proto::HashType hashType,
        const void* input,
        const std::size_t size,
        void* output) const noexcept;
    bool HMAC(
        const proto::HashType hashType,
        const std::uint8_t* input,
        const std::size_t inputSize,
        const std::uint8_t* key,
        const std::size_t keySize,
        std::uint8_t* output) const noexcept;
    auto sha_256_double(const void* input, const std::size_t size, void* output)
        const noexcept -> bool;
    auto sha_256_double_checksum(
        const void* input,
        const std::size_t size,
        void* output) const noexcept -> bool;

    Hash(const Hash&) = delete;
    Hash(Hash&&) = delete;
    Hash& operator=(const Hash&) = delete;
    Hash& operator=(Hash&&) = delete;
};
}  // namespace opentxs::api::crypto::implementation
