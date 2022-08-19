////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022-2022 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "Replication2/StateMachines/Document/DocumentStateTransaction.h"

#include "Transaction/Methods.h"

namespace arangodb::replication2::replicated_state::document {

DocumentStateTransactionResult::DocumentStateTransactionResult(
    arangodb::TransactionId tid, arangodb::OperationResult opRes)
    : _tid(tid), _opRes(std::move(opRes)) {}

auto DocumentStateTransactionResult::ok() const noexcept -> bool {
  return _opRes.ok() && _opRes.countErrorCodes.empty();
}

auto DocumentStateTransactionResult::fail() const noexcept -> bool {
  return !ok();
}

auto DocumentStateTransactionResult::ignoreDuringRecovery() const noexcept
    -> bool {
  if (_opRes.fail() &&
      _opRes.isNot(TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED) &&
      // TODO - should not be necessary once we cancel transactions when we
      // loose leadership
      _opRes.isNot(TRI_ERROR_ARANGO_CONFLICT)) {
    return false;
  }
  for (auto const& it : _opRes.countErrorCodes) {
    if (it.first == TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED ||
        // TODO - should not be necessary once we cancel transactions when we
        // loose leadership
        it.first == TRI_ERROR_ARANGO_CONFLICT) {
      continue;
    }
    return false;
  }
  return true;
}

auto DocumentStateTransactionResult::result() const noexcept -> Result {
  ErrorCode e{_opRes.result.errorNumber()};
  std::stringstream msg;
  if (!_opRes.countErrorCodes.empty()) {
    if (e == TRI_ERROR_NO_ERROR) {
      e = TRI_ERROR_TRANSACTION_INTERNAL;
    }
    msg << "Transaction " << _tid << ": ";
    for (auto const& it : _opRes.countErrorCodes) {
      msg << it.first << ' ';
    }
  }
  return Result{e, std::move(msg).str()};
}

DocumentStateTransaction::DocumentStateTransaction(
    std::unique_ptr<transaction::Methods> methods)
    : _methods(std::move(methods)) {}

auto DocumentStateTransaction::apply(DocumentLogEntry const& entry)
    -> DocumentStateTransactionResult {
  // TODO revisit checkUniqueConstraintsInPreflight and waitForSync
  auto opOptions = OperationOptions();
  opOptions.silent = true;
  opOptions.ignoreRevs = true;
  opOptions.isRestore = true;
  // opOptions.checkUniqueConstraintsInPreflight = true;
  opOptions.validate = false;
  opOptions.waitForSync = false;
  opOptions.indexOperationMode = IndexOperationMode::internal;

  auto opRes = std::invoke([&]() {
    switch (entry.operation) {
      case OperationType::kInsert:
        return _methods->insert(entry.shardId, entry.data.slice(), opOptions);
      case OperationType::kUpdate:
        return _methods->update(entry.shardId, entry.data.slice(), opOptions);
      case OperationType::kReplace:
        return _methods->replace(entry.shardId, entry.data.slice(), opOptions);
      case OperationType::kRemove:
        return _methods->remove(entry.shardId, entry.data.slice(), opOptions);
      case OperationType::kTruncate:
        // TODO Think about correctness and efficiency.
        return _methods->truncate(entry.shardId, opOptions);
      default:
        return OperationResult{
            Result{TRI_ERROR_TRANSACTION_INTERNAL,
                   fmt::format(
                       "Transaction of type {} with ID {} could not be applied",
                       to_string(entry.operation), entry.tid.id())},
            opOptions};
    }
  });
  return DocumentStateTransactionResult{_methods->tid(), std::move(opRes)};
}

auto DocumentStateTransaction::commit() -> Result { return _methods->commit(); }

auto DocumentStateTransaction::abort() -> Result { return _methods->abort(); }

}  // namespace arangodb::replication2::replicated_state::document
