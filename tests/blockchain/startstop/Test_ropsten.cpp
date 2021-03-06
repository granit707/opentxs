// Copyright (c) 2010-2020 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Helpers.hpp"

namespace
{
TEST_F(Test_StartStop, init_opentxs) {}

TEST_F(Test_StartStop, ropsten)
{
    EXPECT_FALSE(
        api_.Blockchain().Start(b::Type::Ethereum_ropsten, "127.0.0.2"));
    EXPECT_FALSE(api_.Blockchain().Stop(b::Type::Ethereum_ropsten));
}
}  // namespace
