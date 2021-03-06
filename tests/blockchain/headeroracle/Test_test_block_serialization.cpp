// Copyright (c) 2010-2020 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Helpers.hpp"

TEST_F(Test_HeaderOracle, test_block_serialization)
{
    const auto empty = ot::Data::Factory();

    ASSERT_TRUE(make_test_block(BLOCK_1, empty));

    const auto hash1 = get_block_hash(BLOCK_1);

    EXPECT_FALSE(hash1->empty());

    auto header = get_test_block(BLOCK_1);

    ASSERT_TRUE(header);
    EXPECT_EQ(header->Hash(), hash1);
    EXPECT_EQ(header->ParentHash(), empty);

    auto serialized = header->Serialize();
    header = api_.Factory().BlockHeader(serialized);

    ASSERT_TRUE(header);
    EXPECT_EQ(header->Hash(), hash1);
    EXPECT_EQ(header->ParentHash(), empty);
    ASSERT_TRUE(make_test_block(BLOCK_2, hash1));

    const auto hash2 = get_block_hash(BLOCK_2);

    EXPECT_FALSE(hash2->empty());

    header = get_test_block(BLOCK_2);

    ASSERT_TRUE(header);
    EXPECT_EQ(header->Hash(), hash2);
    EXPECT_EQ(header->ParentHash(), hash1);

    serialized = header->Serialize();
    header = api_.Factory().BlockHeader(serialized);

    ASSERT_TRUE(header);
    EXPECT_EQ(header->Hash(), hash2);
    EXPECT_EQ(header->ParentHash(), hash1);
}
