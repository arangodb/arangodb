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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "Transaction/ManagedContext.h"

#include "Transaction/Manager.h"
#include "Transaction/ManagerFeature.h"
#include "StorageEngine/TransactionState.h"

namespace arangodb::transaction {
ManagedContext::ManagedContext(TransactionId globalId,
                               std::shared_ptr<TransactionState> state,
                               bool responsibleForCommit, bool cloned)
    : SmartContext(state->vocbase(), globalId, state),
      _responsibleForCommit(responsibleForCommit),
      _cloned(cloned),
      _isSideUser(false) {}

ManagedContext::ManagedContext(TransactionId globalId,
                               std::shared_ptr<TransactionState> state,
                               TransactionContextSideUser /*sideUser*/)
    : SmartContext(state->vocbase(), globalId, state),
      _responsibleForCommit(false),
      _cloned(true),
      _isSideUser(true) {}

ManagedContext::~ManagedContext() {
  bool doReturn = false;

  if (_state != nullptr && !_cloned) {
    TRI_ASSERT(!_responsibleForCommit);
    TRI_ASSERT(!_isSideUser);
    doReturn = true;
  } else if (_isSideUser) {
    TRI_ASSERT(!_responsibleForCommit);
    TRI_ASSERT(_cloned);
    doReturn = true;
  }

  if (doReturn) {
    // we are responsible for returning the lease for the managed transaction
    transaction::Manager* mgr = transaction::ManagerFeature::manager();
    TRI_ASSERT(mgr != nullptr);
    mgr->returnManagedTrx(_globalId, _isSideUser);
  }
}

/// @brief get transaction state, determine commit responsiblity
/*virtual*/ std::shared_ptr<TransactionState>
transaction::ManagedContext::acquireState(transaction::Options const& options,
                                          bool& responsibleForCommit) {
  TRI_ASSERT(_state);
  // single document transaction should never be leased out
  TRI_ASSERT(!_state->hasHint(Hints::Hint::SINGLE_OPERATION));
  responsibleForCommit = _responsibleForCommit;
  return _state;
}

void ManagedContext::unregisterTransaction() noexcept {
  TRI_ASSERT(_responsibleForCommit);
  _state = nullptr;
}

std::shared_ptr<transaction::Context> ManagedContext::clone() const {
  // cloned transactions may never be responsible for commits
  auto clone = std::make_shared<transaction::ManagedContext>(
      _globalId, _state,
      /*responsibleForCommit*/ false, /*cloned*/ true);
  TRI_ASSERT(clone->_state == _state);
  return clone;
}
}  // namespace arangodb::transaction
