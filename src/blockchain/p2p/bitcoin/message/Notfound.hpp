// Copyright (c) 2010-2020 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

namespace opentxs::blockchain::p2p::bitcoin::message::implementation
{
class Notfound final : public internal::Notfound
{
public:
    const value_type& at(const std::size_t position) const noexcept(false) final
    {
        return payload_.at(position);
    }
    const_iterator begin() const noexcept final
    {
        return const_iterator(this, 0);
    }
    const_iterator end() const noexcept final
    {
        return const_iterator(this, payload_.size());
    }
    std::size_t size() const noexcept final { return payload_.size(); }

    ~Notfound() final = default;

private:
    friend opentxs::Factory;

    const std::vector<value_type> payload_;

    OTData payload() const noexcept final;

    Notfound(
        const api::internal::Core& api,
        const blockchain::Type network,
        std::vector<value_type>&& payload) noexcept;
    Notfound(
        const api::internal::Core& api,
        std::unique_ptr<Header> header,
        std::vector<value_type>&& payload) noexcept;
    Notfound(const Notfound&) = delete;
    Notfound(Notfound&&) = delete;
    Notfound& operator=(const Notfound&) = delete;
    Notfound& operator=(Notfound&&) = delete;
};
}  // namespace opentxs::blockchain::p2p::bitcoin::message::implementation
