// Copyright (c) 2010-2020 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

namespace opentxs::network::zeromq::socket::implementation
{
class Pull final : public Receiver<zeromq::socket::Pull>,
                   public zeromq::curve::implementation::Server
{
public:
    ~Pull() final;

private:
    friend opentxs::Factory;

    const ListenCallback& callback_;

    Pull* clone() const noexcept final;
    bool have_callback() const noexcept final;

    void process_incoming(const Lock& lock, Message& message) noexcept final;

    Pull(
        const zeromq::Context& context,
        const Socket::Direction direction,
        const zeromq::ListenCallback& callback,
        const bool startThread) noexcept;
    Pull(
        const zeromq::Context& context,
        const Socket::Direction direction,
        const zeromq::ListenCallback& callback) noexcept;
    Pull(
        const zeromq::Context& context,
        const Socket::Direction direction) noexcept;
    Pull() = delete;
    Pull(const Pull&) = delete;
    Pull(Pull&&) = delete;
    Pull& operator=(const Pull&) = delete;
    Pull& operator=(Pull&&) = delete;
};
}  // namespace opentxs::network::zeromq::socket::implementation
