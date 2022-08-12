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
////////////////////////////////////////////////////////////////////////////////

#include "Replication2/StateMachines/Document/DocumentStateTransaction.h"

#include "Transaction/Methods.h"

namespace arangodb::replication2::replicated_state::document {

DocumentStateTransaction::DocumentStateTransaction(
    std::unique_ptr<transaction::Methods> methods)
    : _methods(std::move(methods)) {}

auto DocumentStateTransaction::apply(DocumentLogEntry const& entry)
    -> futures::Future<OperationResult> {
  // TODO revisit checkUniqueConstraintsInPreflight and waitForSync
  auto opOptions = OperationOptions();
  opOptions.silent = true;
  opOptions.ignoreRevs = true;
  opOptions.isRestore = true;
  // opOptions.checkUniqueConstraintsInPreflight = true;
  opOptions.validate = false;
  opOptions.waitForSync = false;
  opOptions.indexOperationMode = IndexOperationMode::internal;

  auto fut =
      futures::Future<OperationResult>{std::in_place, Result{}, opOptions};
  switch (entry.operation) {
    case OperationType::kInsert:
      fut = _methods->insertAsync(entry.shardId, entry.data.slice(), opOptions);
      break;
    case OperationType::kUpdate:
      fut = _methods->updateAsync(entry.shardId, entry.data.slice(), opOptions);
      break;
    case OperationType::kReplace:
      fut =
          _methods->replaceAsync(entry.shardId, entry.data.slice(), opOptions);
      break;
    case OperationType::kRemove:
      fut = _methods->removeAsync(entry.shardId, entry.data.slice(), opOptions);
      break;
    case OperationType::kTruncate:
      // TODO Think about correctness and efficiency.
      fut = _methods->truncateAsync(entry.shardId, opOptions);
      break;
    default:
      return OperationResult{
          Result{TRI_ERROR_TRANSACTION_INTERNAL,
                 fmt::format(
                     "Transaction of type {} with ID {} could not be applied",
                     to_string(entry.operation), entry.tid.id())},
          opOptions};
  }

  TRI_ASSERT(fut.isReady()) << entry;
  return std::move(fut).get();
}

auto DocumentStateTransaction::commit() -> futures::Future<Result> {
  return _methods->commitAsync();
}

auto DocumentStateTransaction::abort() -> futures::Future<Result> {
  return _methods->abortAsync();
}

}  // namespace arangodb::replication2::replicated_state::document
