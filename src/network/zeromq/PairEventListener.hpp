// Copyright (c) 2010-2020 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "Internal.hpp"

#include "network/zeromq/socket/Subscribe.hpp"

namespace opentxs::network::zeromq::implementation
{
class PairEventListener final : public zeromq::socket::implementation::Subscribe
{
public:
    ~PairEventListener() final = default;

private:
    friend zeromq::implementation::Context;
    typedef socket::implementation::Subscribe ot_super;

    const int instance_;

    PairEventListener* clone() const noexcept final;

    PairEventListener(
        const zeromq::Context& context,
        const zeromq::PairEventCallback& callback,
        const int instance);
    PairEventListener() = delete;
    PairEventListener(const PairEventListener&) = delete;
    PairEventListener(PairEventListener&&) = delete;
    PairEventListener& operator=(const PairEventListener&) = delete;
    PairEventListener& operator=(PairEventListener&&) = delete;
};
}  // namespace opentxs::network::zeromq::implementation
