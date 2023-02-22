////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#include "Basics/debugging.h"
#include "Transaction/Methods.h"

namespace arangodb::replication2::replicated_state::document {

DocumentStateTransaction::DocumentStateTransaction(
    std::unique_ptr<transaction::Methods> methods)
    : _methods(std::move(methods)) {}

auto DocumentStateTransaction::apply(ReplicatedOperation const& op)
    -> OperationResult {
  // TODO revisit checkUniqueConstraintsInPreflight and waitForSync
  auto opOptions = OperationOptions();
  opOptions.silent = true;
  opOptions.ignoreRevs = true;
  opOptions.isRestore = true;
  // opOptions.checkUniqueConstraintsInPreflight = true;
  opOptions.validate = false;
  opOptions.waitForSync = false;
  opOptions.indexOperationMode = IndexOperationMode::internal;

  return std::visit(
      [this, &op, &opOptions](auto&& operation) -> OperationResult {
        using T = std::decay_t<decltype(operation)>;
        if constexpr (IsAnyOf<T, ReplicatedOperation::Insert,
                              ReplicatedOperation::Update,
                              ReplicatedOperation::Replace>) {
          opOptions.overwriteMode = OperationOptions::OverwriteMode::Replace;
          return _methods->insert(operation.shard, operation.payload.slice(),
                                  opOptions);
        } else if constexpr (std::is_same_v<T, ReplicatedOperation::Remove>) {
          return _methods->remove(operation.shard, operation.payload.slice(),
                                  opOptions);
        } else if constexpr (std::is_same_v<T, ReplicatedOperation::Truncate>) {
          // TODO Think about correctness and efficiency.
          return _methods->truncate(operation.shard, opOptions);
        } else {
          TRI_ASSERT(false) << op;
          return OperationResult{
              Result{TRI_ERROR_TRANSACTION_INTERNAL,
                     fmt::format("Operation {} cannot be applied", op)},
              opOptions};
        }
      },
      op.operation);
}

auto DocumentStateTransaction::intermediateCommit() -> Result {
  return _methods->triggerIntermediateCommit();
}

auto DocumentStateTransaction::commit() -> Result { return _methods->commit(); }

auto DocumentStateTransaction::abort() -> Result { return _methods->abort(); }

}  // namespace arangodb::replication2::replicated_state::document
