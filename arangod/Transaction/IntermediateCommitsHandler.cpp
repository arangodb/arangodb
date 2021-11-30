////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "IntermediateCommitsHandler.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Hints.h"
#include "Transaction/Methods.h"

using namespace arangodb;

namespace arangodb::transaction {
   
IntermediateCommitsHandler::IntermediateCommitsHandler(Methods* trx, DataSourceId id)
    : _trx(trx), 
      _id(id), 
      _isResponsibleForCommit(false) {}

IntermediateCommitsHandler::~IntermediateCommitsHandler() {
  // turn intermediate commits back on if required
  restorePreviousState();
}

void IntermediateCommitsHandler::suppressIntermediateCommits() {
  TRI_ASSERT(!_isResponsibleForCommit);
  if (_trx->state()->hasHint(Hints::Hint::INTERMEDIATE_COMMITS)) {
    // temporarily disable intermediate commits
    _trx->state()->unsetHint(Hints::Hint::INTERMEDIATE_COMMITS);
    _isResponsibleForCommit = true;
  }
}
  
Result IntermediateCommitsHandler::commitIfRequired() {
  Result res;
  if (_isResponsibleForCommit) {
    TRI_ASSERT(!_trx->state()->hasHint(Hints::Hint::INTERMEDIATE_COMMITS));
    restorePreviousState();
    TRI_ASSERT(_trx->state()->hasHint(Hints::Hint::INTERMEDIATE_COMMITS));
    res = commit();
  }
  return res;
}

void IntermediateCommitsHandler::restorePreviousState() noexcept {
  if (_isResponsibleForCommit) {
    // restore previous hint
    _trx->state()->setHint(Hints::Hint::INTERMEDIATE_COMMITS);
    _isResponsibleForCommit = false;
  }
}

}  // namespace arangodb::transaction
