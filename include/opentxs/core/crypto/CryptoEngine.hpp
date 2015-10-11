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

#ifndef OPENTXS_CORE_CRYPTO_CRYPTOENGINE_HPP
#define OPENTXS_CORE_CRYPTO_CRYPTOENGINE_HPP

#include <opentxs/core/crypto/CryptoAsymmetric.hpp>
#include <opentxs/core/crypto/CryptoSymmetric.hpp>
#include <opentxs/core/crypto/CryptoUtil.hpp>

#ifdef OT_CRYPTO_USING_OPENSSL
#include <opentxs/core/crypto/OTCryptoOpenSSL.hpp>
#else // Apparently NO crypto engine is defined!
// Perhaps error out here...
#endif

namespace opentxs
{

// Choose your OpenSSL-compatible library here.
#ifdef OT_CRYPTO_USING_OPENSSL
typedef OTCrypto_OpenSSL SSLImplementation;
#else // Apparently NO crypto engine is defined!
// Perhaps error out here...
#endif

//Singlton class for providing an interface to external crypto libraries
//and hold the state required by those libraries.
class CryptoEngine
{
private:
    CryptoEngine();
    CryptoEngine(CryptoEngine const&) = delete;
    CryptoEngine& operator=(CryptoEngine const&) = delete;
    void Init();
    SSLImplementation* pSSL_;
    static CryptoEngine* pInstance_;

public:
    //Utility class for misc OpenSSL-provided functions
    EXPORT CryptoUtil& Util();
    //Asymmetric encryption engines
    EXPORT CryptoAsymmetric& RSA();
    //Symmetric encryption engines
    EXPORT CryptoSymmetric& AES();

    EXPORT static CryptoEngine& Instance();
    void Cleanup();
    ~CryptoEngine();
};

}  // namespace opentxs
#endif // OPENTXS_CORE_CRYPTO_CRYPTOENGINE_HPP
