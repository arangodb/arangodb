////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#include "CalculationTransactionState.h"

#include "Aql/Ast.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/SmartContext.h"
#include "Transaction/Status.h"
#include "VocBase/vocbase.h"

namespace arangodb::aql {

CalculationTransactionState::CalculationTransactionState(
    TRI_vocbase_t& vocbase, transaction::OperationOrigin trxType)
    : TransactionState(vocbase, arangodb::TransactionId(0),
                       arangodb::transaction::Options(), trxType) {
  updateStatus(arangodb::transaction::Status::RUNNING);  // always running to
                                                         // make ASSERTS happy
}

CalculationTransactionState::~CalculationTransactionState() {
  if (status() == arangodb::transaction::Status::RUNNING) {
    updateStatus(
        arangodb::transaction::Status::ABORTED);  // simulate state changes to
                                                  // make ASSERTS happy
  }
}

/// @brief begin a transaction
futures::Future<Result> CalculationTransactionState::beginTransaction(
    arangodb::transaction::Hints) {
  return Result{};
}

/// @brief commit a transaction
futures::Future<arangodb::Result>
CalculationTransactionState::commitTransaction(
    arangodb::transaction::Methods*) {
  applyBeforeCommitCallbacks();
  updateStatus(
      arangodb::transaction::Status::COMMITTED);  // simulate state changes to
                                                  // make ASSERTS happy
  applyAfterCommitCallbacks();
  return Result{};
}

/// @brief abort a transaction
arangodb::Result CalculationTransactionState::abortTransaction(
    arangodb::transaction::Methods*) {
  updateStatus(
      arangodb::transaction::Status::ABORTED);  // simulate state changes to
                                                // make ASSERTS happy
  return {};
}

Result CalculationTransactionState::triggerIntermediateCommit() {
  ADB_PROD_ASSERT(false) << "triggerIntermediateCommit is not supported in "
                            "CalculationTransactionState";
  return Result{TRI_ERROR_INTERNAL};
}

futures::Future<Result>
CalculationTransactionState::performIntermediateCommitIfRequired(
    arangodb::DataSourceId collectionId) {
  // Analyzers do not write. so do nothing
  return Result{};
}

void CalculationTransactionState::addIntermediateCommits(uint64_t /*value*/) {
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

std::unique_ptr<arangodb::TransactionCollection>
CalculationTransactionState::createTransactionCollection(
    arangodb::DataSourceId cid, arangodb::AccessMode::Type accessType) {
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
}  // namespace arangodb::aql