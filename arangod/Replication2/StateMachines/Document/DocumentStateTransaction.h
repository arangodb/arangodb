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
#pragma once

#include "Replication2/StateMachines/Document/DocumentLogEntry.h"

#include "Basics/Result.h"
#include "Utils/OperationResult.h"

#include <memory>

namespace arangodb::transaction {
class Methods;
}

namespace arangodb::replication2::replicated_state::document {

class DocumentStateTransactionResult {
 public:
  explicit DocumentStateTransactionResult(TransactionId tid,
                                          OperationResult opRes);
  [[nodiscard]] auto ok() const noexcept -> bool;
  [[nodiscard]] auto fail() const noexcept -> bool;
  [[nodiscard]] auto ignoreDuringRecovery() const noexcept -> bool;
  [[nodiscard]] auto result() const noexcept -> Result;

 private:
  TransactionId _tid;
  OperationResult _opRes;
};

struct IDocumentStateTransaction {
  virtual ~IDocumentStateTransaction() = default;

  [[nodiscard]] virtual auto apply(DocumentLogEntry const& entry)
      -> DocumentStateTransactionResult = 0;
  [[nodiscard]] virtual auto commit() -> Result = 0;
  [[nodiscard]] virtual auto abort() -> Result = 0;
};

class DocumentStateTransaction
    : public IDocumentStateTransaction,
      public std::enable_shared_from_this<DocumentStateTransaction> {
 public:
  explicit DocumentStateTransaction(
      std::unique_ptr<transaction::Methods> methods);
  auto apply(DocumentLogEntry const& entry)
      -> DocumentStateTransactionResult override;
  auto commit() -> Result override;
  auto abort() -> Result override;

 private:
  std::unique_ptr<transaction::Methods> _methods;
};

}  // namespace arangodb::replication2::replicated_state::document
