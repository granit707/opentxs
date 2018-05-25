/************************************************************
 *
 *                 OPEN TRANSACTIONS
 *
 *       Financial Cryptography and Digital Cash
 *       Library, Protocol, API, Server, CLI, GUI
 *
 *       -- Anonymous Numbered Accounts.
 *       -- Untraceable Digital Cash.
 *       -- Triple-Signed Receipts.
 *       -- Cheques, Vouchers, Transfers, Inboxes.
 *       -- Basket Currencies, Markets, Payment Plans.
 *       -- Signed, XML, Ricardian-style Contracts.
 *       -- Scripted smart contracts.
 *
 *  EMAIL:
 *  fellowtraveler@opentransactions.org
 *
 *  WEBSITE:
 *  http://www.opentransactions.org/
 *
 *  -----------------------------------------------------
 *
 *   LICENSE:
 *   This Source Code Form is subject to the terms of the
 *   Mozilla Public License, v. 2.0. If a copy of the MPL
 *   was not distributed with this file, You can obtain one
 *   at http://mozilla.org/MPL/2.0/.
 *
 *   DISCLAIMER:
 *   This program is distributed in the hope that it will
 *   be useful, but WITHOUT ANY WARRANTY; without even the
 *   implied warranty of MERCHANTABILITY or FITNESS FOR A
 *   PARTICULAR PURPOSE.  See the Mozilla Public License
 *   for more details.
 *
 ************************************************************/

#include "stdafx.hpp"

#include "RequestSocket.hpp"
#include "opentxs/core/Log.hpp"
#include "opentxs/network/zeromq/FrameIterator.hpp"
#include "opentxs/network/zeromq/Frame.hpp"
#include "opentxs/network/zeromq/Message.hpp"
#include "opentxs/OT.hpp"

#include <zmq.h>
#include "Message.hpp"

template class opentxs::Pimpl<opentxs::network::zeromq::RequestSocket>;

#define POLL_MILLISECONDS 1000

#define OT_METHOD "opentxs::network::zeromq::implementation::RequestSocket::"

namespace opentxs::network::zeromq
{
OTZMQRequestSocket RequestSocket::Factory(const class Context& context)
{
    return OTZMQRequestSocket(
        new implementation::RequestSocket(context, OT::Running()));
}
}  // namespace opentxs::network::zeromq

namespace opentxs::network::zeromq::implementation
{
RequestSocket::RequestSocket(
    const zeromq::Context& context,
    const Flag& running)
    : ot_super(context, SocketType::Request)
    , CurveClient(lock_, socket_)
    , running_(running)
{
}

RequestSocket* RequestSocket::clone() const
{
    return new RequestSocket(context_, running_);
}

Socket::MultipartSendResult RequestSocket::SendRequest(
    opentxs::Data& input) const
{
    return SendRequest(Message::Factory(input));
}

Socket::MultipartSendResult RequestSocket::SendRequest(
    const std::string& input) const
{
    auto copy = input;

    return SendRequest(Message::Factory(copy));
}

Socket::MultipartSendResult RequestSocket::SendRequest(
    zeromq::Message& request) const
{
    OT_ASSERT(nullptr != socket_);

    Lock lock(lock_);
    MultipartSendResult output{SendResult::ERROR, Message::Factory()};
    auto& status = output.first;
    auto& reply = output.second;
    bool sent{true};
    const auto parts = request.size();
    std::size_t counter{0};

    for (auto& frame : request) {
        int flags{0};

        if (++counter < parts) { flags = ZMQ_SNDMORE; }

        sent |= (-1 != zmq_msg_send(frame, socket_, flags));
    }

    if (false == sent) {
        otErr << OT_METHOD << __FUNCTION__ << ": Send error:\n"
              << zmq_strerror(zmq_errno()) << std::endl;

        return output;
    }

    const auto ready = wait(lock);

    if (false == ready) {
        otErr << OT_METHOD << __FUNCTION__ << ": Receive timeout." << std::endl;
        status = SendResult::TIMEOUT;

        return output;
    }

    bool receiving{true};

    while (receiving) {
        auto& frame = reply->AddFrame();
        const bool received = (-1 != zmq_msg_recv(frame, socket_, 0));

        if (false == received) {
            otErr << OT_METHOD << __FUNCTION__
                  << ": Receive error: " << zmq_strerror(zmq_errno())
                  << std::endl;

            return output;
        }

        int option{0};
        std::size_t optionBytes{sizeof(option)};

        const bool haveOption =
            (-1 != zmq_getsockopt(socket_, ZMQ_RCVMORE, &option, &optionBytes));

        if (false == haveOption) {
            otErr << OT_METHOD << __FUNCTION__
                  << ": Failed to check socket options error:\n"
                  << zmq_strerror(zmq_errno()) << std::endl;

            return output;
        }

        OT_ASSERT(optionBytes == sizeof(option))

        if (1 != option) { receiving = false; }
    }

    status = SendResult::VALID_REPLY;

    return output;
}

bool RequestSocket::SetCurve(const ServerContract& contract) const
{
    return set_curve(contract);
}

bool RequestSocket::SetSocksProxy(const std::string& proxy) const
{
    return set_socks_proxy(proxy);
}

bool RequestSocket::Start(const std::string& endpoint) const
{
    Lock lock(lock_);

    return start_client(lock, endpoint);
}

bool RequestSocket::wait(const Lock& lock) const
{
    OT_ASSERT(verify_lock(lock))

    const auto start = std::chrono::system_clock::now();
    zmq_pollitem_t poll[1];

    while (running_) {
        poll[0].socket = socket_;
        poll[0].events = ZMQ_POLLIN;
        const auto events = zmq_poll(poll, 1, POLL_MILLISECONDS);

        if (0 == events) {
            otInfo << OT_METHOD << __FUNCTION__ << ": No messages."
                   << std::endl;
            const auto now = std::chrono::system_clock::now();

            if ((now - start) > std::chrono::milliseconds(receive_timeout_)) {

                return false;
            } else {
                continue;
            }
        }

        if (0 > events) {
            const auto error = zmq_errno();
            otErr << OT_METHOD << __FUNCTION__
                  << ": Poll error: " << zmq_strerror(error) << std::endl;

            return false;
        }

        return true;
    }

    return false;
}
}  // namespace opentxs::network::zeromq::implementation
