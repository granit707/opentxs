// Copyright (c) 2010-2020 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "stdafx.hpp"

#include "opentxs/core/Log.hpp"

#include "internal/blockchain/Blockchain.hpp"

#include "BlockFilter.hpp"

// #define OT_METHOD
// "opentxs::api::client::blockchain::database::implementation::BlockFilter::"

namespace opentxs::api::client::blockchain::database::implementation
{
BlockFilter::BlockFilter(
    [[maybe_unused]] const api::internal::Core& api,
    opentxs::storage::lmdb::LMDB& lmdb) noexcept(false)
    : lmdb_(lmdb)
{
}

auto BlockFilter::HaveFilter(const FilterType type, const ReadView blockHash)
    const noexcept -> bool
{
    try {
        return lmdb_.Exists(translate_filter(type), blockHash);
    } catch (...) {

        return false;
    }
}

auto BlockFilter::HaveFilterHeader(
    const FilterType type,
    const ReadView blockHash) const noexcept -> bool
{
    try {
        return lmdb_.Exists(translate_header(type), blockHash);
    } catch (...) {

        return false;
    }
}

auto BlockFilter::LoadFilterHash(
    const FilterType type,
    const ReadView blockHash,
    const AllocateOutput filterHash) const noexcept -> bool
{
    auto output{false};
    auto cb = [&output, &filterHash](const auto in) {
        if ((nullptr == in.data()) || (0 == in.size())) { return; }

        auto proto = proto::BlockchainFilterHeader{};
        proto.ParseFromArray(in.data(), in.size());
        const auto& field = proto.hash();
        auto bytes = filterHash(field.size());

        if (bytes.valid(field.size())) {
            std::memcpy(bytes, field.data(), bytes);
            output = true;
        }
    };

    try {
        lmdb_.Load(translate_header(type), blockHash, cb);
    } catch (...) {
    }

    return output;
}

auto BlockFilter::LoadFilterHeader(
    const FilterType type,
    const ReadView blockHash,
    const AllocateOutput header) const noexcept -> bool
{
    auto output{false};
    auto cb = [&output, &header](const auto in) {
        if ((nullptr == in.data()) || (0 == in.size())) { return; }

        auto proto = proto::BlockchainFilterHeader{};
        proto.ParseFromArray(in.data(), in.size());
        const auto& field = proto.header();
        auto bytes = header(field.size());

        if (bytes.valid(field.size())) {
            std::memcpy(bytes, field.data(), bytes);
            output = true;
        }
    };

    try {
        lmdb_.Load(translate_header(type), blockHash, cb);
    } catch (...) {
    }

    return output;
}

auto BlockFilter::StoreFilterHeaders(
    const FilterType type,
    const std::vector<FilterHeader>& headers) const noexcept -> bool
{
    auto parentTxn = lmdb_.TransactionRW();

    for (const auto& [block, header, hash] : headers) {
        auto proto = proto::BlockchainFilterHeader();
        proto.set_version(1);
        proto.set_header(header->str());
        proto.set_hash(std::string{hash});
        auto bytes = space(proto.ByteSize());
        proto.SerializeWithCachedSizesToArray(
            reinterpret_cast<std::uint8_t*>(bytes.data()));

        try {
            const auto stored = lmdb_.Store(
                translate_header(type),
                block->Bytes(),
                reader(bytes),
                parentTxn);

            if (false == stored.first) { return false; }
        } catch (...) {

            return false;
        }
    }

    return parentTxn.Finalize(true);
}

auto BlockFilter::StoreFilters(
    const FilterType type,
    std::vector<FilterData>& filters) const noexcept -> bool
{
    auto parentTxn = lmdb_.TransactionRW();

    for (auto& [block, pFilter] : filters) {
        OT_ASSERT(pFilter);

        const auto& filter = *pFilter;
        const auto proto = filter.Serialize();
        auto bytes = space(proto.ByteSize());
        proto.SerializeWithCachedSizesToArray(
            reinterpret_cast<std::uint8_t*>(bytes.data()));

        try {
            const auto stored = lmdb_.Store(
                translate_filter(type), block, reader(bytes), parentTxn);

            if (false == stored.first) { return false; }
        } catch (...) {

            return false;
        }
    }

    return parentTxn.Finalize(true);
}

auto BlockFilter::translate_filter(const FilterType type) noexcept(false)
    -> Table
{
    switch (type) {
        case FilterType::Basic_BIP158: {
            return FiltersBasic;
        }
        case FilterType::Basic_BCHVariant: {
            return FiltersBCH;
        }
        case FilterType::Extended_opentxs: {
            return FiltersOpentxs;
        }
        default: {
            throw std::runtime_error("Unsupported filter type");
        }
    }
}

auto BlockFilter::translate_header(const FilterType type) noexcept(false)
    -> Table
{
    switch (type) {
        case FilterType::Basic_BIP158: {
            return FilterHeadersBasic;
        }
        case FilterType::Basic_BCHVariant: {
            return FilterHeadersBCH;
        }
        case FilterType::Extended_opentxs: {
            return FilterHeadersOpentxs;
        }
        default: {
            throw std::runtime_error("Unsupported filter type");
        }
    }
}
}  // namespace opentxs::api::client::blockchain::database::implementation
