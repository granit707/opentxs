// Copyright (c) 2010-2020 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "stdafx.hpp"

#include "Internal.hpp"

#include "opentxs/core/Log.hpp"
#include "opentxs/Bytes.hpp"

#include <array>

const std::map<opentxs::blockchain::Type, opentxs::proto::ContactItemType>
    type_map_{
        {opentxs::blockchain::Type::Unknown, opentxs::proto::CITEMTYPE_UNKNOWN},
        {opentxs::blockchain::Type::Bitcoin, opentxs::proto::CITEMTYPE_BTC},
        {opentxs::blockchain::Type::Bitcoin_testnet3,
         opentxs::proto::CITEMTYPE_TNBTC},
        {opentxs::blockchain::Type::BitcoinCash, opentxs::proto::CITEMTYPE_BCH},
        {opentxs::blockchain::Type::BitcoinCash_testnet3,
         opentxs::proto::CITEMTYPE_TNBCH},
        {opentxs::blockchain::Type::Ethereum_frontier,
         opentxs::proto::CITEMTYPE_ETH},
        {opentxs::blockchain::Type::Ethereum_ropsten,
         opentxs::proto::CITEMTYPE_ETHEREUM_ROPSTEN},
        {opentxs::blockchain::Type::Litecoin, opentxs::proto::CITEMTYPE_LTC},
        {opentxs::blockchain::Type::Litecoin_testnet4,
         opentxs::proto::CITEMTYPE_TNLTC},
    };
const std::map<opentxs::proto::ContactItemType, opentxs::blockchain::Type>
    type_reverse_map_{opentxs::reverse_map(type_map_)};

namespace opentxs
{
bool operator==(
    const opentxs::ProtobufType& lhs,
    const opentxs::ProtobufType& rhs) noexcept
{
    auto sLeft = std::string{};
    auto sRight = std::string{};
    lhs.SerializeToString(&sLeft);
    rhs.SerializeToString(&sRight);

    return sLeft == sRight;
}

proto::ContactItemType Translate(const blockchain::Type type) noexcept
{
    try {
        return type_map_.at(type);
    } catch (...) {
        return proto::CITEMTYPE_UNKNOWN;
    }
}

blockchain::Type Translate(const proto::ContactItemType type) noexcept
{
    try {
        return type_reverse_map_.at(type);
    } catch (...) {
        return blockchain::Type::Unknown;
    }
}

auto reader(const Space& in) noexcept -> ReadView
{
    return {reinterpret_cast<const char*>(in.data()), in.size()};
}
auto reader(const WritableView& in) noexcept -> ReadView
{
    return {in.as<const char>(), in.size()};
}
auto space(const std::size_t size) noexcept -> Space
{
    auto output = Space{};
    output.assign(size, std::byte{51});

    return output;
}
auto writer(std::string& in) noexcept -> AllocateOutput
{
    return [&in](const auto size) -> WritableView {
        in.resize(size, 51);

        return {in.data(), in.size()};
    };
}
auto writer(Space& in) noexcept -> AllocateOutput
{
    return [&in](const auto size) -> WritableView {
        in.resize(size, std::byte{51});

        return {in.data(), in.size()};
    };
}
}  // namespace opentxs
