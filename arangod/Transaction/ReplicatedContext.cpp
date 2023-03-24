////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Alexandru Petenchea
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#include "Transaction/ReplicatedContext.h"

#include "Transaction/Manager.h"
#include "Transaction/ManagerFeature.h"
#include "StorageEngine/TransactionState.h"

namespace arangodb::transaction {

ReplicatedContext::ReplicatedContext(TransactionId globalId,
                                     std::shared_ptr<TransactionState> state)
    : SmartContext(state->vocbase(), globalId, state){};

std::shared_ptr<TransactionState> ReplicatedContext::acquireState(
    Options const& options, bool& responsibleForCommit) {
  TRI_ASSERT(_state);
  responsibleForCommit = true;
  return _state;
}

void ReplicatedContext::unregisterTransaction() noexcept { _state = nullptr; }

}  // namespace arangodb::transaction
