// Copyright (c) 2010-2020 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "Internal.hpp"

namespace opentxs::api::client::implementation
{
class Workflow final : opentxs::api::client::Workflow, Lockable
{
public:
    bool AbortTransfer(
        const identifier::Nym& nymID,
        const Item& transfer,
        const Message& reply) const final;
    bool AcceptTransfer(
        const identifier::Nym& nymID,
        const identifier::Server& notaryID,
        const OTTransaction& pending,
        const Message& reply) const final;
    bool AcknowledgeTransfer(
        const identifier::Nym& nymID,
        const Item& transfer,
        const Message& reply) const final;
#if OT_CASH
    OTIdentifier AllocateCash(
        const identifier::Nym& id,
        const blind::Purse& purse) const final;
#endif
    bool CancelCheque(
        const opentxs::Cheque& cheque,
        const Message& request,
        const Message* reply) const final;
    bool ClearCheque(
        const identifier::Nym& recipientNymID,
        const OTTransaction& receipt) const final;
    bool ClearTransfer(
        const identifier::Nym& nymID,
        const identifier::Server& notaryID,
        const OTTransaction& receipt) const final;
    bool CompleteTransfer(
        const identifier::Nym& nymID,
        const identifier::Server& notaryID,
        const OTTransaction& receipt,
        const Message& reply) const final;
    OTIdentifier ConveyTransfer(
        const identifier::Nym& nymID,
        const identifier::Server& notaryID,
        const OTTransaction& pending) const final;
    OTIdentifier CreateTransfer(const Item& transfer, const Message& request)
        const final;
    bool DepositCheque(
        const identifier::Nym& nymID,
        const Identifier& accountID,
        const opentxs::Cheque& cheque,
        const Message& request,
        const Message* reply) const final;
    bool ExpireCheque(
        const identifier::Nym& nymID,
        const opentxs::Cheque& cheque) const final;
    bool ExportCheque(const opentxs::Cheque& cheque) const final;
    bool FinishCheque(
        const opentxs::Cheque& cheque,
        const Message& request,
        const Message* reply) const final;
    OTIdentifier ImportCheque(
        const identifier::Nym& nymID,
        const opentxs::Cheque& cheque) const final;
    std::set<OTIdentifier> List(
        const identifier::Nym& nymID,
        const proto::PaymentWorkflowType type,
        const proto::PaymentWorkflowState state) const final;
    Cheque LoadCheque(const identifier::Nym& nymID, const Identifier& chequeID)
        const final;
    Cheque LoadChequeByWorkflow(
        const identifier::Nym& nymID,
        const Identifier& workflowID) const final;
    Transfer LoadTransfer(
        const identifier::Nym& nymID,
        const Identifier& transferID) const final;
    Transfer LoadTransferByWorkflow(
        const identifier::Nym& nymID,
        const Identifier& workflowID) const final;
    std::shared_ptr<proto::PaymentWorkflow> LoadWorkflow(
        const identifier::Nym& nymID,
        const Identifier& workflowID) const final;
#if OT_CASH
    OTIdentifier ReceiveCash(
        const identifier::Nym& receiver,
        const blind::Purse& purse,
        const Message& message) const final;
#endif
    OTIdentifier ReceiveCheque(
        const identifier::Nym& nymID,
        const opentxs::Cheque& cheque,
        const Message& message) const final;
#if OT_CASH
    bool SendCash(
        const identifier::Nym& sender,
        const identifier::Nym& recipient,
        const Identifier& workflowID,
        const Message& request,
        const Message* reply) const final;
#endif
    bool SendCheque(
        const opentxs::Cheque& cheque,
        const Message& request,
        const Message* reply) const final;
    std::vector<OTIdentifier> WorkflowsByAccount(
        const identifier::Nym& nymID,
        const Identifier& accountID) const final;
    OTIdentifier WriteCheque(const opentxs::Cheque& cheque) const final;

    ~Workflow() final = default;

private:
    friend opentxs::Factory;

    struct ProtobufVersions {
        VersionNumber event_;
        VersionNumber source_;
        VersionNumber workflow_;
    };

    using VersionMap = std::map<proto::PaymentWorkflowType, ProtobufVersions>;

    static const VersionMap versions_;

    const api::internal::Core& api_;
    const Activity& activity_;
    const Contacts& contact_;
    const OTZMQPublishSocket account_publisher_;
    const OTZMQPushSocket rpc_publisher_;
    mutable std::map<std::string, std::shared_mutex> workflow_locks_;

    static bool can_abort_transfer(const proto::PaymentWorkflow& workflow);
    static bool can_accept_cheque(const proto::PaymentWorkflow& workflow);
    static bool can_accept_transfer(const proto::PaymentWorkflow& workflow);
    static bool can_acknowledge_transfer(
        const proto::PaymentWorkflow& workflow);
    static bool can_cancel_cheque(const proto::PaymentWorkflow& workflow);
    static bool can_clear_transfer(const proto::PaymentWorkflow& workflow);
    static bool can_complete_transfer(const proto::PaymentWorkflow& workflow);
#if OT_CASH
    static bool can_convey_cash(const proto::PaymentWorkflow& workflow);
#endif
    static bool can_convey_cheque(const proto::PaymentWorkflow& workflow);
    static bool can_convey_transfer(const proto::PaymentWorkflow& workflow);
    static bool can_deposit_cheque(const proto::PaymentWorkflow& workflow);
    static bool can_expire_cheque(
        const opentxs::Cheque& cheque,
        const proto::PaymentWorkflow& workflow);
    static bool can_finish_cheque(const proto::PaymentWorkflow& workflow);
    static bool cheque_deposit_success(const Message* message);
    static std::chrono::time_point<std::chrono::system_clock>
    extract_conveyed_time(const proto::PaymentWorkflow& workflow);
    static bool isCheque(const opentxs::Cheque& cheque);
    static bool isTransfer(const Item& item);
    static bool validate_recipient(
        const identifier::Nym& nymID,
        const opentxs::Cheque& cheque);

    bool add_cheque_event(
        const eLock& lock,
        const std::string& nymID,
        const std::string& eventNym,
        proto::PaymentWorkflow& workflow,
        const proto::PaymentWorkflowState newState,
        const proto::PaymentEventType newEventType,
        const VersionNumber version,
        const Message& request,
        const Message* reply,
        const std::string& account = "") const;
    bool add_cheque_event(
        const eLock& lock,
        const std::string& nymID,
        const std::string& accountID,
        proto::PaymentWorkflow& workflow,
        const proto::PaymentWorkflowState newState,
        const proto::PaymentEventType newEventType,
        const VersionNumber version,
        const identifier::Nym& recipientNymID,
        const OTTransaction& receipt,
        const Time time = Clock::now()) const;
    bool add_transfer_event(
        const eLock& lock,
        const std::string& nymID,
        const std::string& eventNym,
        proto::PaymentWorkflow& workflow,
        const proto::PaymentWorkflowState newState,
        const proto::PaymentEventType newEventType,
        const VersionNumber version,
        const Message& message,
        const std::string& account,
        const bool success) const;
    bool add_transfer_event(
        const eLock& lock,
        const std::string& nymID,
        const std::string& notaryID,
        const std::string& eventNym,
        proto::PaymentWorkflow& workflow,
        const proto::PaymentWorkflowState newState,
        const proto::PaymentEventType newEventType,
        const VersionNumber version,
        const OTTransaction& receipt,
        const std::string& account,
        const bool success) const;
    OTIdentifier convey_incoming_transfer(
        const identifier::Nym& nymID,
        const identifier::Server& notaryID,
        const OTTransaction& pending,
        const std::string& senderNymID,
        const std::string& recipientNymID,
        const Item& transfer) const;
    OTIdentifier convey_internal_transfer(
        const identifier::Nym& nymID,
        const identifier::Server& notaryID,
        const OTTransaction& pending,
        const std::string& senderNymID,
        const Item& transfer) const;
    std::pair<OTIdentifier, proto::PaymentWorkflow> create_cheque(
        const Lock& global,
        const std::string& nymID,
        const opentxs::Cheque& cheque,
        const proto::PaymentWorkflowType workflowType,
        const proto::PaymentWorkflowState workflowState,
        const VersionNumber workflowVersion,
        const VersionNumber sourceVersion,
        const VersionNumber eventVersion,
        const std::string& party,
        const std::string& account,
        const Message* message = nullptr) const;
    std::pair<OTIdentifier, proto::PaymentWorkflow> create_transfer(
        const Lock& global,
        const std::string& nymID,
        const Item& transfer,
        const proto::PaymentWorkflowType workflowType,
        const proto::PaymentWorkflowState workflowState,
        const VersionNumber workflowVersion,
        const VersionNumber sourceVersion,
        const VersionNumber eventVersion,
        const std::string& party,
        const std::string& account,
        const std::string& notaryID,
        const std::string& destinationAccountID) const;
    std::unique_ptr<Item> extract_transfer_from_pending(
        const OTTransaction& receipt) const;
    std::unique_ptr<Item> extract_transfer_from_receipt(
        const OTTransaction& receipt,
        Identifier& depositorNymID) const;
    template <typename T>
    std::shared_ptr<proto::PaymentWorkflow> get_workflow(
        const Lock& global,
        const std::set<proto::PaymentWorkflowType>& types,
        const std::string& nymID,
        const T& source) const;
    std::shared_ptr<proto::PaymentWorkflow> get_workflow_by_id(
        const std::set<proto::PaymentWorkflowType>& types,
        const std::string& nymID,
        const std::string& workflowID) const;
    std::shared_ptr<proto::PaymentWorkflow> get_workflow_by_id(
        const std::string& nymID,
        const std::string& workflowID) const;
    std::shared_ptr<proto::PaymentWorkflow> get_workflow_by_source(
        const std::set<proto::PaymentWorkflowType>& types,
        const std::string& nymID,
        const std::string& sourceID) const;
    // Unlocks global after successfully locking the workflow-specific mutex
    eLock get_workflow_lock(Lock& global, const std::string& id) const;
    bool isInternalTransfer(
        const Identifier& sourceAccount,
        const Identifier& destinationAccount) const;
    bool save_workflow(
        const std::string& nymID,
        const proto::PaymentWorkflow& workflow) const;
    bool save_workflow(
        const std::string& nymID,
        const std::string& accountID,
        const proto::PaymentWorkflow& workflow) const;
    OTIdentifier save_workflow(
        OTIdentifier&& workflowID,
        const std::string& nymID,
        const std::string& accountID,
        const proto::PaymentWorkflow& workflow) const;
    std::pair<OTIdentifier, proto::PaymentWorkflow> save_workflow(
        std::pair<OTIdentifier, proto::PaymentWorkflow>&& workflowID,
        const std::string& nymID,
        const std::string& accountID,
        const proto::PaymentWorkflow& workflow) const;
    bool update_activity(
        const identifier::Nym& localNymID,
        const identifier::Nym& remoteNymID,
        const Identifier& sourceID,
        const Identifier& workflowID,
        const StorageBox type,
        std::chrono::time_point<std::chrono::system_clock> time) const;
    void update_rpc(
        const std::string& localNymID,
        const std::string& remoteNymID,
        const std::string& accountID,
        const proto::AccountEventType type,
        const std::string& workflowID,
        const Amount amount,
        const Amount pending,
        const std::chrono::time_point<std::chrono::system_clock> time,
        const std::string& memo) const;

    Workflow(
        const api::internal::Core& api,
        const Activity& activity,
        const Contacts& contact);
    Workflow() = delete;
    Workflow(const Workflow&) = delete;
    Workflow(Workflow&&) = delete;
    Workflow& operator=(const Workflow&) = delete;
    Workflow& operator=(Workflow&&) = delete;
};
}  // namespace opentxs::api::client::implementation
