# Copyright (c) 2010-2020 The Open-Transactions developers
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

add_subdirectory(blockchain)

set(
  cxx-sources
  Activity.cpp
  Blockchain.cpp
  Contacts.cpp
  Factory.cpp
  Issuer.cpp
  Manager.cpp
  OTX.cpp
  Pair.cpp
  ServerAction.cpp
  UI.cpp
  Wallet.cpp
  Workflow.cpp
)
set(
  cxx-install-headers
  "${opentxs_SOURCE_DIR}/include/opentxs/api/client/Activity.hpp"
  "${opentxs_SOURCE_DIR}/include/opentxs/api/client/Blockchain.hpp"
  "${opentxs_SOURCE_DIR}/include/opentxs/api/client/Contacts.hpp"
  "${opentxs_SOURCE_DIR}/include/opentxs/api/client/Issuer.hpp"
  "${opentxs_SOURCE_DIR}/include/opentxs/api/client/Manager.hpp"
  "${opentxs_SOURCE_DIR}/include/opentxs/api/client/OTX.hpp"
  "${opentxs_SOURCE_DIR}/include/opentxs/api/client/Pair.hpp"
  "${opentxs_SOURCE_DIR}/include/opentxs/api/client/ServerAction.hpp"
  "${opentxs_SOURCE_DIR}/include/opentxs/api/client/UI.hpp"
  "${opentxs_SOURCE_DIR}/include/opentxs/api/client/Workflow.hpp"
)
set(
  cxx-headers
  ${cxx-install-headers}
  "${opentxs_SOURCE_DIR}/src/internal/api/client/Client.hpp"
  Activity.hpp
  Blockchain.hpp
  Contacts.hpp
  Factory.hpp
  Issuer.hpp
  Manager.hpp
  OTX.hpp
  Pair.hpp
  ServerAction.hpp
  UI.hpp
  Wallet.hpp
  Workflow.hpp
)

add_library(opentxs-api-client OBJECT ${cxx-sources} ${cxx-headers})

if(OT_BLOCKCHAIN_EXPORT)
  target_include_directories(
    opentxs-api-client SYSTEM
    PRIVATE ${LMDB_INCLUDE_DIR}
  )
endif()

target_link_libraries(opentxs-api-client PRIVATE Boost::headers)
set_property(TARGET opentxs-api-client PROPERTY POSITION_INDEPENDENT_CODE 1)

install(
  FILES ${cxx-install-headers}
  DESTINATION include/opentxs/api/client
  COMPONENT dev
)
