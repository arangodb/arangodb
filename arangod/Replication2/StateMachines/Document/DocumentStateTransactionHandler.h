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

#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/StateMachines/Document/DocumentLogEntry.h"

#include "Transaction/Options.h"
#include "Utils/DatabaseGuard.h"
#include "VocBase/Identifiers/TransactionId.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace arangodb::replication2::replicated_state::document {

struct IDocumentStateTransaction;
class DocumentStateTransaction;

struct IDocumentStateTransactionHandler {
  virtual ~IDocumentStateTransactionHandler() = default;
  virtual auto applyEntry(DocumentLogEntry doc) -> Result = 0;
  virtual auto ensureTransaction(DocumentLogEntry doc)
      -> std::shared_ptr<IDocumentStateTransaction> = 0;
  virtual void removeTransaction(TransactionId tid) = 0;
};

class DocumentStateTransactionHandler
    : public IDocumentStateTransactionHandler {
 public:
  explicit DocumentStateTransactionHandler(GlobalLogIdentifier gid,
                                           DatabaseFeature& databaseFeature);
  auto applyEntry(DocumentLogEntry doc) -> Result override;
  auto ensureTransaction(DocumentLogEntry doc)
      -> std::shared_ptr<IDocumentStateTransaction> override;
  void removeTransaction(TransactionId tid) override;

 private:
  auto getTrx(TransactionId tid) -> std::shared_ptr<DocumentStateTransaction>;

 private:
  GlobalLogIdentifier _gid;
  DatabaseGuard _db;
  std::unordered_map<TransactionId, std::shared_ptr<DocumentStateTransaction>>
      _transactions;
};

}  // namespace arangodb::replication2::replicated_state::document
