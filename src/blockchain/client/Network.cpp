// Copyright (c) 2010-2020 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "stdafx.hpp"

#include "opentxs/api/Core.hpp"
#include "opentxs/api/Endpoints.hpp"
#include "opentxs/blockchain/block/Header.hpp"
#include "opentxs/core/Log.hpp"
#include "opentxs/network/zeromq/socket/Sender.tpp"
#include "opentxs/network/zeromq/Frame.hpp"
#include "opentxs/network/zeromq/FrameIterator.hpp"
#include "opentxs/network/zeromq/FrameSection.hpp"
#include "opentxs/network/zeromq/Message.hpp"

#include "internal/api/Api.hpp"

#include "Network.hpp"

#define OT_METHOD "opentxs::blockchain::client::implementation::Network::"

namespace opentxs::blockchain::client::implementation
{
Network::Network(
    const api::internal::Core& api,
    const api::client::internal::Blockchain& blockchain,
    const Type type,
    const std::string& seednode,
    const std::string& shutdown) noexcept
    : Executor(api)
    , shutdown_sender_(api.ZeroMQ(), shutdown_endpoint())
    , database_p_(Factory::BlockchainDatabase(
          api,
          *this,
          blockchain.BlockchainDB(),
          type))
    , header_p_(Factory::HeaderOracle(api, *this, *database_p_, type))
    , peer_p_(Factory::BlockchainPeerManager(
          api,
          *this,
          *database_p_,
          blockchain.IO(),
          type,
          seednode,
          shutdown_sender_.endpoint_))
    , filter_p_(Factory::BlockchainFilterOracle(
          api,
          *this,
          *database_p_,
          type,
          shutdown_sender_.endpoint_))
    , wallet_p_()
    , chain_(type)
    , database_(*database_p_)
    , filters_(*filter_p_)
    , header_(*header_p_)
    , peer_(*peer_p_)
    , parent_(blockchain)
    , local_chain_height_(0)
    , remote_chain_height_(0)
    , processing_headers_(Flag::Factory(false))
    , task_id_(-1)
{
    OT_ASSERT(database_p_);
    OT_ASSERT(filter_p_);
    OT_ASSERT(header_p_);
    OT_ASSERT(peer_p_);

    init_executor({});
}

auto Network::AddPeer(const p2p::Address& address) const noexcept -> bool
{
    if (false == running_.get()) { return false; }

    return peer_.AddPeer(address);
}

auto Network::Connect() noexcept -> bool
{
    if (false == running_.get()) { return false; }

    return peer_.Connect();
}

auto Network::Disconnect() noexcept -> bool
{
    // TODO

    return false;
}

auto Network::GetConfirmations(const std::string& txid) const noexcept
    -> ChainHeight
{
    // TODO

    return -1;
}

auto Network::GetPeerCount() const noexcept -> std::size_t
{
    if (false == running_.get()) { return false; }

    return peer_.GetPeerCount();
}

auto Network::init() noexcept -> void
{
    local_chain_height_.store(header_.BestChain().first);

    {
        const auto best = database_.CurrentBest();

        OT_ASSERT(best);

        const auto position = best->Position();
        LogNormal(blockchain::internal::DisplayString(chain_))(
            " chain initialized with best hash ")(position.second->asHex())(
            " at height ")(position.first)
            .Flush();
    }

    peer_.init();
    filters_.Start();
    task_id_ = api_.Schedule(std::chrono::seconds(30), [this]() { Trigger(); });
    Trigger();
}

auto Network::pipeline(zmq::Message& in) noexcept -> void
{
    if (false == running_.get()) { return; }

    const auto header = in.Header();

    OT_ASSERT(0 < header.size());

    switch (header.at(0).as<Task>()) {
        case Task::SubmitBlockHeader: {
            process_header(in);
        } break;
        case Task::SubmitFilterHeader: {
            process_cfheader(in);
        } break;
        case Task::SubmitFilter: {
            process_filter(in);
        } break;
        case Task::StateMachine: {
            process_state_machine();
        } break;
        case Task::Shutdown: {
            shutdown(shutdown_promise_);
        } break;
        default: {
            OT_FAIL;
        }
    }
}

auto Network::process_cfheader(network::zeromq::Message& in) noexcept -> void
{
    if (false == running_.get()) { return; }

    auto type = filter::Type{};
    auto stopBlock = ReadView{};
    auto previousHeader = ReadView{};
    auto hashes = std::vector<ReadView>{};

    {
        auto counter{0};
        const auto body = in.Body();

        if (body.size() < 3) {
            LogOutput(OT_METHOD)(__FUNCTION__)(": Invalid message").Flush();

            return;
        }

        for (const auto& frame : in.Body()) {
            switch (++counter) {
                case 1: {
                    type = frame.as<filter::Type>();
                } break;
                case 2: {
                    stopBlock = frame.Bytes();
                } break;
                case 3: {
                    previousHeader = frame.Bytes();
                } break;
                default: {
                    hashes.emplace_back(frame.Bytes());
                }
            }
        }
    }

    filters_.AddHeaders(type, stopBlock, previousHeader, std::move(hashes));
}

auto Network::process_filter(network::zeromq::Message& in) noexcept -> void
{
    if (false == running_.get()) { return; }

    const auto body = in.Body();

    if (3 != body.size()) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Invalid message").Flush();

        return;
    }

    filters_.AddFilter(
        body.at(0).as<filter::Type>(),
        Data::Factory(body.at(1)),
        Data::Factory(body.at(2)));
}

auto Network::process_header(network::zeromq::Message& in) noexcept -> void
{
    struct Cleanup {
        Cleanup(Flag& flag)
            : flag_(flag)
        {
            flag_.On();
        }

        ~Cleanup() { flag_.Off(); }

    private:
        Flag& flag_;
    };

    if (false == running_.get()) { return; }

    auto cleanup = Cleanup(processing_headers_);
    using Promise = std::promise<void>;
    auto pPromise = std::unique_ptr<Promise>{};
    auto input = std::vector<ReadView>{};

    {
        auto counter{0};
        const auto body = in.Body();

        if (body.size() < 2) {
            LogOutput(OT_METHOD)(__FUNCTION__)(": Invalid message").Flush();

            return;
        }

        for (const auto& frame : in.Body()) {
            switch (++counter) {
                case 1: {
                    pPromise.reset(
                        reinterpret_cast<Promise*>(frame.as<std::uintptr_t>()));
                } break;
                default: {
                    input.emplace_back(frame.Bytes());
                }
            }
        }
    }

    OT_ASSERT(pPromise);

    auto& promise = *pPromise;
    auto headers = std::vector<std::unique_ptr<block::Header>>{};

    for (const auto& header : input) {
        headers.emplace_back(instantiate_header(header));
    }

    header_.AddHeaders(headers);
    processing_headers_->Off();
    promise.set_value();
    Trigger();
}

auto Network::process_state_machine() noexcept -> void
{
    filters_.CheckBlocks();
    peer_.Run();

    try {
        if (IsSynchronized()) {
            state_machine_.set_value(false);
        } else {
            if (false == processing_headers_.get()) { peer_.RequestHeaders(); }

            Sleep(std::chrono::milliseconds(20));

            state_machine_.set_value(true);
        }
    } catch (...) {
    }
}

auto Network::RequestFilterHeaders(
    const filter::Type type,
    const block::Height start,
    const block::Hash& stop) const noexcept -> void
{
    if (false == running_.get()) { return; }

    peer_.RequestFilterHeaders(type, start, stop);
}

auto Network::RequestFilters(
    const filter::Type type,
    const block::Height start,
    const block::Hash& stop) const noexcept -> void
{
    if (false == running_.get()) { return; }

    peer_.RequestFilters(type, start, stop);
}

auto Network::SendToAddress(
    const std::string& address,
    const Amount amount,
    const api::client::blockchain::BalanceTree& source) const noexcept
    -> std::string
{
    // TODO

    return {};
}

auto Network::SendToPaymentCode(
    const std::string& address,
    const Amount amount,
    const api::client::blockchain::PaymentCode& source) const noexcept
    -> std::string
{
    // TODO

    return {};
}

auto Network::shutdown(std::promise<void>& promise) noexcept -> void
{
    if (running_->Off()) {
        try {
            state_machine_.set_value(false);
        } catch (...) {
        }

        api_.Cancel(task_id_);
        shutdown_sender_.Activate();
        peer_.Shutdown().get();
        filters_.Shutdown().get();
        shutdown_sender_.Close();

        try {
            promise.set_value();
        } catch (...) {
        }
    }
}

auto Network::shutdown_endpoint() noexcept -> std::string
{
    return std::string{"inproc://"} + Identifier::Random()->str();
}

auto Network::Submit(network::zeromq::Message& work) const noexcept -> void
{
    if (false == running_.get()) { return; }

    pipeline_->Push(work);
}

auto Network::UpdateHeight(const block::Height height) const noexcept -> void
{
    if (false == running_.get()) { return; }

    remote_chain_height_.store(std::max(height, remote_chain_height_.load()));
    Trigger();
}

auto Network::UpdateLocalHeight(const block::Position position) const noexcept
    -> void
{
    if (false == running_.get()) { return; }

    const auto& [height, hash] = position;
    LogNormal(blockchain::internal::DisplayString(chain_))(
        " block header chain updated to hash ")(hash->asHex())(" at height ")(
        height)
        .Flush();
    local_chain_height_.store(height);
    Trigger();
}

auto Network::Work(const Task type) const noexcept -> OTZMQMessage
{
    auto output = api_.ZeroMQ().Message(type);
    output->AddFrame();

    return output;
}

Network::~Network() { Shutdown().get(); }
}  // namespace opentxs::blockchain::client::implementation
