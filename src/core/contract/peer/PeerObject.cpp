// Copyright (c) 2010-2020 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "stdafx.hpp"

#include "Internal.hpp"

#include "opentxs/api/client/Contacts.hpp"
#include "opentxs/api/Core.hpp"
#include "opentxs/api/Factory.hpp"
#include "opentxs/api/Wallet.hpp"
#if OT_CASH
#include "opentxs/blind/Purse.hpp"
#endif
#include "opentxs/core/contract/peer/PeerObject.hpp"
#include "opentxs/core/contract/peer/PeerReply.hpp"
#include "opentxs/core/contract/peer/PeerRequest.hpp"
#include "opentxs/core/contract/Signable.hpp"
#include "opentxs/core/identifier/Nym.hpp"
#include "opentxs/core/Armored.hpp"
#include "opentxs/core/Log.hpp"
#include "opentxs/core/String.hpp"
#include "opentxs/crypto/Envelope.hpp"
#include "opentxs/Proto.tpp"

#include "internal/api/Api.hpp"

#include "PeerObject.hpp"

#define OT_METHOD "opentxs::peer::implementation::Object::"

namespace opentxs
{
opentxs::PeerObject* Factory::PeerObject(
    const api::internal::Core& api,
    const Nym_p& senderNym,
    const std::string& message)
{
    try {
        std::unique_ptr<opentxs::PeerObject> output(
            new peer::implementation::Object(api, senderNym, message));

        if (!output->Validate()) { output.reset(); }

        return output.release();
    } catch (const std::exception& e) {
        LogOutput("opentxs::Factory::")(__FUNCTION__)(": ")(e.what()).Flush();

        return {};
    }
}

opentxs::PeerObject* Factory::PeerObject(
    const api::internal::Core& api,
    const Nym_p& senderNym,
    const std::string& payment,
    const bool isPayment)
{
    try {
        if (!isPayment) { return Factory::PeerObject(api, senderNym, payment); }

        std::unique_ptr<opentxs::PeerObject> output(
            new peer::implementation::Object(api, payment, senderNym));

        if (!output->Validate()) { output.reset(); }

        return output.release();
    } catch (const std::exception& e) {
        LogOutput("opentxs::Factory::")(__FUNCTION__)(": ")(e.what()).Flush();

        return {};
    }
}

#if OT_CASH
opentxs::PeerObject* Factory::PeerObject(
    const api::internal::Core& api,
    const Nym_p& senderNym,
    const std::shared_ptr<blind::Purse> purse)
{
    try {
        std::unique_ptr<opentxs::PeerObject> output(
            new peer::implementation::Object(api, senderNym, purse));

        if (!output->Validate()) { output.reset(); }

        return output.release();
    } catch (const std::exception& e) {
        LogOutput("opentxs::Factory::")(__FUNCTION__)(": ")(e.what()).Flush();

        return {};
    }
}
#endif

opentxs::PeerObject* Factory::PeerObject(
    const api::internal::Core& api,
    const OTPeerRequest request,
    const OTPeerReply reply,
    const VersionNumber version)
{
    try {
        std::unique_ptr<opentxs::PeerObject> output(
            new peer::implementation::Object(api, request, reply, version));

        if (!output->Validate()) { output.reset(); }

        return output.release();
    } catch (const std::exception& e) {
        LogOutput("opentxs::Factory::")(__FUNCTION__)(": ")(e.what()).Flush();

        return {};
    }
}

opentxs::PeerObject* Factory::PeerObject(
    const api::internal::Core& api,
    const OTPeerRequest request,
    const VersionNumber version)
{
    try {
        std::unique_ptr<opentxs::PeerObject> output(
            new peer::implementation::Object(api, request, version));

        if (!output->Validate()) { output.reset(); }

        return output.release();
    } catch (const std::exception& e) {
        LogOutput("opentxs::Factory::")(__FUNCTION__)(": ")(e.what()).Flush();

        return {};
    }
}

opentxs::PeerObject* Factory::PeerObject(
    const api::client::Contacts& contacts,
    const api::internal::Core& api,
    const Nym_p& signerNym,
    const proto::PeerObject& serialized)
{
    try {
        const bool valid = proto::Validate(serialized, VERBOSE);
        std::unique_ptr<opentxs::PeerObject> output;

        if (valid) {
            output.reset(new peer::implementation::Object(
                contacts, api, signerNym, serialized));
        } else {
            LogOutput(OT_METHOD)(__FUNCTION__)(": Invalid peer object.")
                .Flush();
        }

        if (!output->Validate()) { output.reset(); }

        return output.release();
    } catch (const std::exception& e) {
        LogOutput("opentxs::Factory::")(__FUNCTION__)(": ")(e.what()).Flush();

        return {};
    }
}

opentxs::PeerObject* Factory::PeerObject(
    const api::client::Contacts& contacts,
    const api::internal::Core& api,
    const Nym_p& recipientNym,
    const opentxs::Armored& encrypted,
    const opentxs::PasswordPrompt& reason)
{
    try {
        auto notUsed = Nym_p{};
        auto output = std::unique_ptr<opentxs::PeerObject>{};
        auto input = api.Factory().Envelope(encrypted);
        auto contents = String::Factory();

        if (false ==
            input->Open(*recipientNym, contents->WriteInto(), reason)) {
            LogOutput("opentxs::Factory::")(__FUNCTION__)(
                ": Unable to decrypt message")
                .Flush();

            return {};
        }

        auto serialized = proto::StringToProto<proto::PeerObject>(contents);

        return Factory::PeerObject(contacts, api, notUsed, serialized);
    } catch (const std::exception& e) {
        LogOutput("opentxs::Factory::")(__FUNCTION__)(": ")(e.what()).Flush();

        return {};
    }
}
}  // namespace opentxs

namespace opentxs::peer::implementation
{
Object::Object(
    const api::internal::Core& api,
    const Nym_p& nym,
    const std::string& message,
    const std::string& payment,
    const OTPeerReply reply,
    const OTPeerRequest request,
#if OT_CASH
    const std::shared_ptr<blind::Purse> purse,
#endif
    const proto::PeerObjectType type,
    const VersionNumber version)
    : api_(api)
    , nym_(nym)
    , message_(message.empty() ? nullptr : new std::string(message))
    , payment_(payment.empty() ? nullptr : new std::string(payment))
    , reply_(reply)
    , request_(request)
#if OT_CASH
    , purse_(purse)
#endif
    , type_(type)
    , version_(version)
{
}

Object::Object(
    const api::client::Contacts& contacts,
    const api::internal::Core& api,
    const Nym_p& signerNym,
    const proto::PeerObject serialized)
    : Object(
          api,
          {},
          {},
          {},
          api.Factory().PeerReply(),
          api.Factory().PeerRequest(),
#if OT_CASH
          {},
#endif
          serialized.type(),
          serialized.version())
{
    Nym_p objectNym{nullptr};

    if (serialized.has_nym()) {
        objectNym = api_.Wallet().Nym(serialized.nym());
        contacts.Update(serialized.nym());
    }

    if (signerNym) {
        nym_ = signerNym;
    } else if (objectNym) {
        nym_ = objectNym;
    }

    switch (serialized.type()) {
        case (proto::PEEROBJECT_MESSAGE): {
            message_.reset(new std::string(serialized.otmessage()));
        } break;
        case (proto::PEEROBJECT_REQUEST): {
            request_ = api_.Factory().PeerRequest(nym_, serialized.otrequest());
        } break;
        case (proto::PEEROBJECT_RESPONSE): {
            if (false == bool(nym_)) {
                nym_ = api_.Wallet().Nym(
                    api_.Factory().NymID(serialized.otrequest().recipient()));
            }

            auto senderNym = api_.Wallet().Nym(
                api_.Factory().NymID(serialized.otrequest().initiator()));
            request_ =
                api_.Factory().PeerRequest(senderNym, serialized.otrequest());
            reply_ = api_.Factory().PeerReply(nym_, serialized.otreply());
        } break;
        case (proto::PEEROBJECT_PAYMENT): {
            payment_.reset(new std::string(serialized.otpayment()));
        } break;
        case (proto::PEEROBJECT_CASH): {
#if OT_CASH
            purse_.reset(opentxs::Factory::Purse(api_, serialized.purse()));
#endif
        } break;
        default: {
            LogOutput(OT_METHOD)(__FUNCTION__)(": Incorrect type.").Flush();
        }
    }
}

Object::Object(
    const api::internal::Core& api,
    const Nym_p& senderNym,
    const std::string& message)
    : Object(
          api,
          senderNym,
          message,
          {},
          api.Factory().PeerReply(),
          api.Factory().PeerRequest(),
#if OT_CASH
          {},
#endif
          proto::PEEROBJECT_MESSAGE,
          PEER_MESSAGE_VERSION)
{
}

#if OT_CASH
Object::Object(
    const api::internal::Core& api,
    const Nym_p& senderNym,
    const std::shared_ptr<blind::Purse> purse)
    : Object(
          api,
          senderNym,
          {},
          {},
          api.Factory().PeerReply(),
          api.Factory().PeerRequest(),
          purse,
          proto::PEEROBJECT_CASH,
          PEER_CASH_VERSION)
{
}
#endif

Object::Object(
    const api::internal::Core& api,
    const std::string& payment,
    const Nym_p& senderNym)
    : Object(
          api,
          senderNym,
          {},
          payment,
          api.Factory().PeerReply(),
          api.Factory().PeerRequest(),
#if OT_CASH
          {},
#endif
          proto::PEEROBJECT_PAYMENT,
          PEER_PAYMENT_VERSION)
{
}

Object::Object(
    const api::internal::Core& api,
    const OTPeerRequest request,
    const OTPeerReply reply,
    const VersionNumber version)
    : Object(
          api,
          {},
          {},
          {},
          reply,
          request,
#if OT_CASH
          {},
#endif
          proto::PEEROBJECT_RESPONSE,
          version)
{
}

Object::Object(
    const api::internal::Core& api,
    const OTPeerRequest request,
    const VersionNumber version)
    : Object(
          api,
          {},
          {},
          {},
          api.Factory().PeerReply(),
          request,
#if OT_CASH
          {},
#endif
          proto::PEEROBJECT_REQUEST,
          version)
{
}

proto::PeerObject Object::Serialize() const
{
    proto::PeerObject output;

    output.set_type(type_);

    switch (type_) {
        case (proto::PEEROBJECT_MESSAGE): {
            if (PEER_MESSAGE_VERSION > version_) {
                output.set_version(PEER_MESSAGE_VERSION);
            } else {
                output.set_version(version_);
            }

            if (message_) {
                if (nym_) { *output.mutable_nym() = nym_->asPublicNym(); }
                output.set_otmessage(String::Factory(*message_)->Get());
            }
        } break;
        case (proto::PEEROBJECT_PAYMENT): {
            if (PEER_PAYMENT_VERSION > version_) {
                output.set_version(PEER_PAYMENT_VERSION);
            } else {
                output.set_version(version_);
            }

            if (payment_) {
                if (nym_) { *output.mutable_nym() = nym_->asPublicNym(); }
                output.set_otpayment(String::Factory(*payment_)->Get());
            }
        } break;
        case (proto::PEEROBJECT_REQUEST): {
            output.set_version(version_);

            if (0 < request_->Version()) {
                *(output.mutable_otrequest()) = request_->Contract();
                auto nym = api_.Wallet().Nym(request_->Initiator());

                if (nym) { *output.mutable_nym() = nym->asPublicNym(); }
            }
        } break;
        case (proto::PEEROBJECT_RESPONSE): {
            output.set_version(version_);

            if (0 < reply_->Version()) {
                *(output.mutable_otreply()) = reply_->Contract();
            }
            if (0 < request_->Version()) {
                *(output.mutable_otrequest()) = request_->Contract();
            }
        } break;
#if OT_CASH
        case (proto::PEEROBJECT_CASH): {
            if (PEER_CASH_VERSION > version_) {
                output.set_version(PEER_CASH_VERSION);
            } else {
                output.set_version(version_);
            }

            if (purse_) {
                if (nym_) { *output.mutable_nym() = nym_->asPublicNym(); }
                *output.mutable_purse() = purse_->Serialize();
            }
        } break;
#endif
        default: {
            LogOutput(OT_METHOD)(__FUNCTION__)(": Unknown type.").Flush();
        }
    }

    return output;
}

bool Object::Validate() const
{
    bool validChildren = false;

    switch (type_) {
        case (proto::PEEROBJECT_MESSAGE): {
            validChildren = bool(message_);
        } break;
        case (proto::PEEROBJECT_REQUEST): {
            if (0 < request_->Version()) {
                validChildren = request_->Validate();
            }
        } break;
        case (proto::PEEROBJECT_RESPONSE): {
            if ((0 == reply_->Version()) || (0 == request_->Version())) {
                break;
            }

            validChildren = reply_->Validate() && request_->Validate();
        } break;
        case (proto::PEEROBJECT_PAYMENT): {
            validChildren = bool(payment_);
        } break;
#if OT_CASH
        case (proto::PEEROBJECT_CASH): {
            validChildren = bool(purse_);
        } break;
#endif
        default: {
            LogOutput(OT_METHOD)(__FUNCTION__)(": Unknown type.").Flush();
        }
    }

    const bool validProto = proto::Validate(Serialize(), VERBOSE);

    return (validChildren && validProto);
}
}  // namespace opentxs::peer::implementation
