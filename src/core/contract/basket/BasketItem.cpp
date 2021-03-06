// Copyright (c) 2010-2020 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "stdafx.hpp"

#include "opentxs/core/contract/basket/BasketItem.hpp"
#include "opentxs/core/Identifier.hpp"

namespace opentxs
{
BasketItem::BasketItem()
    : SUB_CONTRACT_ID(Identifier::Factory())
    , SUB_ACCOUNT_ID(Identifier::Factory())
    , lMinimumTransferAmount(0)
    , lClosingTransactionNo(0)
{
}
}  // namespace opentxs
