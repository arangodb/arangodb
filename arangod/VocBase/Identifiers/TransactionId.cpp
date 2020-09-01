////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#include "VocBase/Identifiers/TransactionId.h"
#include "VocBase/ticks.h"

namespace arangodb {

bool TransactionId::isSet() const noexcept { return id() != 0; }

bool TransactionId::empty() const noexcept { return !isSet(); }

bool TransactionId::isCoordinatorTransactionId() const {
  return (id() % 4) == 0;
}

bool TransactionId::isFollowerTransactionId() const { return (id() % 4) == 2; }

bool TransactionId::isLeaderTransactionId() const { return (id() % 4) == 1; }

bool TransactionId::isChildTransactionId() const {
  return isLeaderTransactionId() || isFollowerTransactionId();
}

bool TransactionId::isLegacyTransactionId() const { return (id() % 4) == 3; }

uint32_t TransactionId::serverId() const {
  return TRI_ExtractServerIdFromTick(id());
}

TransactionId TransactionId::child() { return TransactionId(id() + 1); }

TransactionId TransactionId::createSingleServer() {
  return TransactionId(TRI_NewTickServer());
}

TransactionId TransactionId::createCoordinator() {
  return TransactionId(TRI_NewServerSpecificTickMod4());
}

TransactionId TransactionId::createLegacy() {
  return TransactionId(TRI_NewServerSpecificTickMod4() + 3);
}

}  // namespace arangodb
