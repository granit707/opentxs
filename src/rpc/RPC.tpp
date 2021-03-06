// Copyright (c) 2010-2020 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#define OT_METHOD "opentxs::rpc::implementation::RPC::"

namespace opentxs::rpc::implementation
{
template <typename T>
void RPC::evaluate_register_account(
    const api::client::OTX::Result& result,
    T& output) const
{
    // TODO use structured binding
    // const auto& [status, pReply] = result;
    const auto& status = std::get<0>(result);
    const auto& pReply = std::get<1>(result);

    if (proto::LASTREPLYSTATUS_NOTSENT == status) {
        add_output_status(output, proto::RPCRESPONSE_ERROR);
    } else if (proto::LASTREPLYSTATUS_UNKNOWN == status) {
        add_output_status(output, proto::RPCRESPONSE_BAD_SERVER_RESPONSE);
    } else if (proto::LASTREPLYSTATUS_MESSAGEFAILED == status) {
        add_output_status(output, proto::RPCRESPONSE_REGISTER_ACCOUNT_FAILED);
    } else if (proto::LASTREPLYSTATUS_MESSAGESUCCESS == status) {
        OT_ASSERT(pReply);

        const auto& reply = *pReply;
        add_output_identifier(reply.m_strAcctID->Get(), output);
        add_output_status(output, proto::RPCRESPONSE_SUCCESS);
    }
}

template <typename T>
void RPC::evaluate_register_nym(
    const api::client::OTX::Result& result,
    T& output) const
{
    // TODO use structured binding
    // const auto& [status, pReply] = result;
    const auto& status = std::get<0>(result);

    if (proto::LASTREPLYSTATUS_NOTSENT == status) {
        add_output_status(output, proto::RPCRESPONSE_ERROR);
    } else if (proto::LASTREPLYSTATUS_UNKNOWN == status) {
        add_output_status(output, proto::RPCRESPONSE_BAD_SERVER_RESPONSE);
    } else if (proto::LASTREPLYSTATUS_MESSAGEFAILED == status) {
        add_output_status(output, proto::RPCRESPONSE_REGISTER_NYM_FAILED);
    } else if (proto::LASTREPLYSTATUS_MESSAGESUCCESS == status) {
        add_output_status(output, proto::RPCRESPONSE_SUCCESS);
    }
}

template <typename T>
void RPC::evaluate_transaction_reply(
    const api::client::internal::Manager& client,
    const Message& reply,
    T& output,
    const proto::RPCResponseCode code) const
{
    bool success{true};
    const auto notaryID = client.Factory().ServerID(reply.m_strNotaryID);
    const auto nymID = client.Factory().NymID(reply.m_strNymID);
    const auto accountID = client.Factory().Identifier(reply.m_strAcctID);
    const bool transaction =
        reply.m_strCommand->Compare("notarizeTransactionResponse") ||
        reply.m_strCommand->Compare("processInboxResponse") ||
        reply.m_strCommand->Compare("processNymboxResponse");

    if (transaction) {
        if (const auto sLedger = String::Factory(reply.m_ascPayload);
            sLedger->Exists()) {
            if (auto ledger{
                    client.Factory().Ledger(nymID, accountID, notaryID)};
                ledger->LoadContractFromString(sLedger)) {
                if (ledger->GetTransactionCount() > 0) {
                    for (const auto& [key, value] :
                         ledger->GetTransactionMap()) {
                        if (false == bool(value)) {
                            success = false;
                            break;
                        } else {
                            success &= value->GetSuccess();
                        }
                    }
                } else {
                    success = false;
                }
            } else {
                success = false;
            }
        } else {
            success = false;
        }
    } else {
        success = false;
    }

    if (success) {
        add_output_status(output, proto::RPCRESPONSE_SUCCESS);
    } else {
        add_output_status(output, code);
    }
}
}  // namespace opentxs::rpc::implementation
