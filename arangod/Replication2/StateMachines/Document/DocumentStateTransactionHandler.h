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

#include "Replication2/LoggerContext.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/StateMachines/Document/ReplicatedOperation.h"

#include "Transaction/Options.h"
#include "VocBase/Identifiers/TransactionId.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace arangodb::replication2::replicated_state::document {

struct IDocumentStateTransaction;
struct IDocumentStateHandlersFactory;
struct IDocumentStateShardHandler;
class DocumentStateTransaction;

struct IDocumentStateTransactionHandler {
  using TransactionMap =
      std::unordered_map<TransactionId,
                         std::shared_ptr<IDocumentStateTransaction>>;

  virtual ~IDocumentStateTransactionHandler() = default;
  [[nodiscard]] virtual auto applyEntry(ReplicatedOperation operation)
      -> Result = 0;
  [[nodiscard]] virtual auto applyEntry(
      ReplicatedOperation::OperationType const& operation) -> Result = 0;
  virtual void removeTransaction(TransactionId tid) = 0;
  virtual auto getTransactionsForShard(ShardID const&)
      -> std::vector<TransactionId> = 0;
  [[nodiscard]] virtual auto getUnfinishedTransactions() const
      -> TransactionMap const& = 0;
  [[nodiscard]] virtual auto validate(
      ReplicatedOperation::OperationType const& operation) const -> Result = 0;
};

class DocumentStateTransactionHandler
    : public IDocumentStateTransactionHandler {
 public:
  explicit DocumentStateTransactionHandler(
      GlobalLogIdentifier gid, TRI_vocbase_t* vocbase,
      std::shared_ptr<IDocumentStateHandlersFactory> factory,
      std::shared_ptr<IDocumentStateShardHandler> shardHandler);
  [[nodiscard]] auto applyEntry(ReplicatedOperation operation)
      -> Result override;
  [[nodiscard]] auto applyEntry(
      ReplicatedOperation::OperationType const& operation) -> Result override;
  void removeTransaction(TransactionId tid) override;
  auto getTransactionsForShard(ShardID const&)
      -> std::vector<TransactionId> override;
  [[nodiscard]] auto getUnfinishedTransactions() const
      -> TransactionMap const& override;
  [[nodiscard]] auto validate(
      ReplicatedOperation::OperationType const& operation) const
      -> Result override;

 private:
  auto getTrx(TransactionId tid) -> std::shared_ptr<IDocumentStateTransaction>;
  void setTrx(TransactionId tid,
              std::shared_ptr<IDocumentStateTransaction> trx);

  auto applyOp(FinishesUserTransaction auto const&) -> Result;
  auto applyOp(ReplicatedOperation::IntermediateCommit const&) -> Result;
  auto applyOp(ModifiesUserTransaction auto const&) -> Result;
  auto applyOp(ReplicatedOperation::AbortAllOngoingTrx const&) -> Result;
  auto applyOp(ReplicatedOperation::CreateShard const&) -> Result;
  auto applyOp(ReplicatedOperation::DropShard const&) -> Result;

 private:
  GlobalLogIdentifier _gid;
  TRI_vocbase_t* _vocbase;
  LoggerContext _logContext;
  std::shared_ptr<IDocumentStateHandlersFactory> _factory;
  std::shared_ptr<IDocumentStateShardHandler> _shardHandler;
  TransactionMap _transactions;
};

}  // namespace arangodb::replication2::replicated_state::document
