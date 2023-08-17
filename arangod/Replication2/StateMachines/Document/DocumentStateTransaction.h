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
#pragma once

#include "Basics/Result.h"
#include "Cluster/ClusterTypes.h"
#include "Replication2/StateMachines/Document/ReplicatedOperation.h"
#include "Utils/OperationResult.h"

#include <memory>

namespace arangodb::transaction {
class Methods;
}

namespace arangodb::replication2::replicated_state::document {

struct IDocumentStateTransaction {
  virtual ~IDocumentStateTransaction() = default;

  [[nodiscard]] virtual auto apply(ReplicatedOperation::OperationType const& op)
      -> OperationResult = 0;
  [[nodiscard]] virtual auto intermediateCommit() -> Result = 0;
  [[nodiscard]] virtual auto commit() -> Result = 0;
  [[nodiscard]] virtual auto abort() -> Result = 0;
  [[nodiscard]] virtual auto containsShard(ShardID const&) -> bool = 0;
};

class DocumentStateTransaction
    : public IDocumentStateTransaction,
      public std::enable_shared_from_this<DocumentStateTransaction> {
 public:
  explicit DocumentStateTransaction(
      std::unique_ptr<transaction::Methods> methods);
  [[nodiscard]] auto apply(ReplicatedOperation::OperationType const& op)
      -> OperationResult override;
  auto intermediateCommit() -> Result override;
  auto commit() -> Result override;
  auto abort() -> Result override;
  auto containsShard(ShardID const&) -> bool override;

 private:
  auto buildDefaultOptions() -> OperationOptions;
  auto applyOp(InsertsDocuments auto const&, OperationOptions&)
      -> OperationResult;
  auto applyOp(ReplicatedOperation::Remove const& op, OperationOptions&)
      -> OperationResult;
  auto applyOp(ReplicatedOperation::Truncate const& op, OperationOptions&)
      -> OperationResult;

 private:
  std::unique_ptr<transaction::Methods> _methods;
};

}  // namespace arangodb::replication2::replicated_state::document
