// Copyright (c) 2010-2020 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "stdafx.hpp"

#include "Internal.hpp"

#include "opentxs/api/crypto/Crypto.hpp"
#include "opentxs/api/crypto/Symmetric.hpp"
#include "opentxs/api/network/Dht.hpp"
#include "opentxs/api/Core.hpp"
#include "opentxs/api/Factory.hpp"
#include "opentxs/api/HDSeed.hpp"
#include "opentxs/api/Wallet.hpp"
#include "opentxs/core/crypto/OTCaller.hpp"
#include "opentxs/core/crypto/OTPassword.hpp"
#include "opentxs/core/Flag.hpp"
#include "opentxs/core/Log.hpp"
#include "opentxs/core/PasswordPrompt.hpp"
#include "opentxs/crypto/key/Symmetric.hpp"

#include <algorithm>
#include <cstring>
#include <limits>

#include "Core.hpp"

//#define OT_METHOD "opentxs::api::implementation::Core::"

namespace
{
opentxs::OTCaller* external_password_callback_{nullptr};

extern "C" std::int32_t internal_password_cb(
    char* output,
    std::int32_t size,
    std::int32_t rwflag,
    void* userdata)
{
    OT_ASSERT(nullptr != userdata);
    OT_ASSERT(nullptr != external_password_callback_);

    const bool askTwice = (1 == rwflag);
    const auto& reason = *static_cast<opentxs::PasswordPrompt*>(userdata);
    const auto& api = opentxs::api::implementation::Core::get_api(reason);
    opentxs::Lock lock(api.Lock());
    opentxs::OTPassword secret{};

    if (false == api.GetSecret(lock, secret, reason, askTwice)) {
        opentxs::LogOutput(__FUNCTION__)(": Callback error").Flush();

        return 0;
    }

    std::int32_t len{};
    len = (secret.isPassword()) ? secret.getPasswordSize()
                                : secret.getMemorySize();
    len = std::min(len, size);

    if (len <= 0) {
        opentxs::LogOutput(__FUNCTION__)(": Callback error").Flush();

        return 0;
    }

    std::memcpy(
        output,
        (secret.isPassword()) ? secret.getPassword() : secret.getMemory(),
        len);

    return len;
}
}  // namespace

namespace opentxs::api::implementation
{
Core::Core(
    const api::internal::Context& parent,
    Flag& running,
    const ArgList& args,
    const api::Crypto& crypto,
    const api::Settings& config,
    const opentxs::network::zeromq::Context& zmq,
    const std::string& dataFolder,
    const int instance,
    const bool dhtDefault,
    std::unique_ptr<api::internal::Factory> factory)
    : ZMQ(zmq, instance)
    , Scheduler(parent, running)
    , StorageParent(running, args, crypto, config, parent.Legacy(), dataFolder)
    , factory_p_(std::move(factory))
    , factory_(*factory_p_)
    , asymmetric_(factory_.Asymmetric())
    , symmetric_(factory_.Symmetric())
    , seeds_(opentxs::Factory::HDSeed(
          factory_,
          asymmetric_,
          symmetric_,
          *storage_,
          crypto_.BIP32(),
          crypto_.BIP39()))
    , wallet_(nullptr)
    , dht_(opentxs::Factory::Dht(
          dhtDefault,
          *this,
          nym_publish_interval_,
          nym_refresh_interval_,
          server_publish_interval_,
          server_refresh_interval_,
          unit_publish_interval_,
          unit_refresh_interval_))
    , master_key_lock_()
    , master_secret_()
    , master_key_(make_master_key(
          parent,
          factory_,
          encrypted_secret_,
          master_secret_,
          symmetric_,
          *storage_))
    , password_timeout_()
    , password_duration_(-1)
    , last_activity_()
    , timeout_thread_running_(false)
{
    OT_ASSERT(seeds_);
    OT_ASSERT(dht_);

    if (master_secret_) {
        opentxs::Lock lock(master_key_lock_);
        bump_password_timer(lock);
    }
}

void Core::bump_password_timer(const opentxs::Lock& lock) const
{
    last_activity_ = Clock::now();
    const auto running = timeout_thread_running_.exchange(true);

    if (running) { return; }

    if (password_timeout_.joinable()) { password_timeout_.join(); }

    password_timeout_ = std::thread{&Core::password_timeout, this};
}

void Core::cleanup()
{
    shutdown_sender_.Activate();
    dht_.reset();
    wallet_.reset();
    seeds_.reset();
    factory_p_.reset();

    if (password_timeout_.joinable()) { password_timeout_.join(); }
}

const api::network::Dht& Core::DHT() const
{
    OT_ASSERT(dht_)

    return *dht_;
}

const api::internal::Core& Core::get_api(const PasswordPrompt& reason) noexcept
{
    return reason.api_;
}

INTERNAL_PASSWORD_CALLBACK* Core::GetInternalPasswordCallback() const
{
    return &internal_password_cb;
}

bool Core::GetSecret(
    const opentxs::Lock& lock,
    OTPassword& secret,
    const PasswordPrompt& reason,
    const bool twice) const
{
    bump_password_timer(lock);

    if (master_secret_) {
        secret = *master_secret_;

        return true;
    }

    master_secret_ = std::make_unique<OTPassword>();

    OT_ASSERT(master_secret_);

    auto& callback = *external_password_callback_;
    OTPassword masterPassword{};
    OTPasswordPrompt prompt{reason};

    if (twice) {
        callback.AskTwice(reason, masterPassword);
    } else {
        callback.AskOnce(reason, masterPassword);
    }

    prompt->SetPassword(masterPassword);

    if (false == master_key_->Unlock(prompt)) {
        opentxs::LogOutput(__FUNCTION__)(": Failed to unlock master key")
            .Flush();

        return false;
    }

    const auto decrypted = master_key_->Decrypt(
        encrypted_secret_,
        prompt,
        master_secret_->WriteInto(OTPassword::Mode::Mem));

    if (false == decrypted) {
        opentxs::LogOutput(__FUNCTION__)(": Failed to decrypt master secret")
            .Flush();

        return false;
    }

    secret = *master_secret_;

    return true;
}

OTSymmetricKey Core::make_master_key(
    const api::internal::Context& parent,
    const api::Factory& factory,
    const proto::Ciphertext& encrypted_secret,
    std::unique_ptr<OTPassword>& master_secret,
    const api::crypto::Symmetric& symmetric,
    const api::storage::Storage& storage)
{
    auto& caller = parent.GetPasswordCaller();
    external_password_callback_ = &caller;

    OT_ASSERT(nullptr != external_password_callback_);

    auto& encrypted = const_cast<proto::Ciphertext&>(encrypted_secret);
    std::shared_ptr<proto::Ciphertext> existing{};
    const auto have = storage.Load(existing, true);

    if (have) {
        encrypted = *existing;

        return symmetric.Key(existing->key(), proto::SMODE_CHACHA20POLY1305);
    }

    master_secret = std::make_unique<OTPassword>();

    OT_ASSERT(master_secret);

    master_secret->randomizeMemory(32);

    auto reason = factory.PasswordPrompt("Choose a master password");
    OTPassword masterPassword{};
    caller.AskTwice(reason, masterPassword);
    reason->SetPassword(masterPassword);
    auto output = symmetric.Key(reason, proto::SMODE_CHACHA20POLY1305);
    auto saved = output->Encrypt(
        master_secret->Bytes(),
        reason,
        encrypted,
        true,
        proto::SMODE_CHACHA20POLY1305);

    OT_ASSERT(saved);

    saved = storage.Store(encrypted);

    OT_ASSERT(saved);

    return output;
}

const opentxs::crypto::key::Symmetric& Core::MasterKey(
    const opentxs::Lock& lock) const
{
    return master_key_;
}

const api::HDSeed& Core::Seeds() const
{
    OT_ASSERT(seeds_);

    return *seeds_;
}

void Core::SetMasterKeyTimeout(const std::chrono::seconds& timeout) const
{
    opentxs::Lock lock(master_key_lock_);
    password_duration_ = timeout;
}

const api::storage::Storage& Core::Storage() const
{
    OT_ASSERT(storage_)

    return *storage_;
}

void Core::storage_gc_hook()
{
    if (storage_) { storage_->RunGC(); }
}

void Core::password_timeout() const
{
    struct Cleanup {
        std::atomic<bool>& running_;

        Cleanup(std::atomic<bool>& running)
            : running_(running)
        {
            running_.store(true);
        }

        ~Cleanup() { running_.store(false); }
    };

    opentxs::Lock lock(master_key_lock_, std::defer_lock);
    Cleanup cleanup(timeout_thread_running_);

    while (running_) {
        lock.lock();

        // Negative durations means never time out
        if (0 > password_duration_.count()) { return; }

        const auto now = Clock::now();
        const auto interval = now - last_activity_;

        if (interval > password_duration_) {
            master_secret_.reset();

            return;
        }

        lock.unlock();
        Sleep(std::chrono::milliseconds(250));
    }
}

const api::Wallet& Core::Wallet() const
{
    OT_ASSERT(wallet_);

    return *wallet_;
}

Core::~Core() { cleanup(); }
}  // namespace opentxs::api::implementation
