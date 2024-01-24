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

#include "Replication2/StateMachines/Document/DocumentStateTransactionHandler.h"

#include "Basics/application-exit.h"
#include "Basics/voc-errors.h"
#include "Logger/LogContextKeys.h"
#include "Replication2/StateMachines/Document/DocumentStateHandlersFactory.h"
#include "Replication2/StateMachines/Document/DocumentStateShardHandler.h"
#include "Replication2/StateMachines/Document/DocumentStateTransaction.h"
#include "VocBase/AccessMode.h"

namespace arangodb::replication2::replicated_state::document {

DocumentStateTransactionHandler::DocumentStateTransactionHandler(
    GlobalLogIdentifier gid, TRI_vocbase_t* vocbase,
    std::shared_ptr<IDocumentStateHandlersFactory> factory,
    std::shared_ptr<IDocumentStateShardHandler> shardHandler)
    : _gid(std::move(gid)),
      _vocbase(vocbase),
      _loggerContext(LoggerContext(Logger::REPLICATED_STATE)
                         .with<logContextKeyDatabaseName>(_gid.database)
                         .with<logContextKeyLogId>(_gid.id)),
      _factory(std::move(factory)),
      _shardHandler(std::move(shardHandler)),
      _errorHandler(_factory->createErrorHandler(_gid)) {
#ifndef ARANGODB_USE_GOOGLE_TESTS
  TRI_ASSERT(_vocbase != nullptr);
#endif
}

auto DocumentStateTransactionHandler::getTrx(TransactionId tid)
    -> std::shared_ptr<IDocumentStateTransaction> {
  return _transactions.doUnderLock(
      [&](auto& transactions) -> std::shared_ptr<IDocumentStateTransaction> {
        auto it = transactions.find(tid);
        if (it == transactions.end()) {
          return nullptr;
        }
        return it->second;
      });
}

void DocumentStateTransactionHandler::setTrx(
    TransactionId tid, std::shared_ptr<IDocumentStateTransaction> trx) {
  auto [_, isInserted] =
      _transactions.getLockedGuard()->emplace(tid, std::move(trx));
  ADB_PROD_ASSERT(isInserted)
      << "Transaction " << tid << " already exists (gid " << _gid << ")";
}

void DocumentStateTransactionHandler::removeTransaction(TransactionId tid) {
  _transactions.getLockedGuard()->erase(tid);
}

auto DocumentStateTransactionHandler::getUnfinishedTransactions() const
    -> TransactionMap {
  return _transactions.copy();
}

auto DocumentStateTransactionHandler::getTransactionsForShard(
    ShardID const& sid) -> std::vector<TransactionId> {
  std::vector<TransactionId> result;
  _transactions.doUnderLock([&](auto& transactions) {
    for (auto const& [tid, trx] : transactions) {
      if (trx->containsShard(sid)) {
        result.emplace_back(tid);
      }
    }
  });
  return result;
}

auto DocumentStateTransactionHandler::applyOp(
    FinishesUserTransaction auto const& op) -> Result {
  TRI_ASSERT(op.tid.isFollowerTransactionId());
  auto trx = getTrx(op.tid);
  ADB_PROD_ASSERT(trx != nullptr)
      << "Transaction " << op.tid << " not found for operation " << op
      << " (gid " << _gid << ")";

  auto res = std::invoke(
      overload{
          [&](ReplicatedOperation::Commit const&) { return trx->commit(); },
          [&](ReplicatedOperation::Abort const&) { return trx->abort(); }},
      op);

  removeTransaction(op.tid);
  return res;
}

auto DocumentStateTransactionHandler::applyOp(
    ReplicatedOperation::IntermediateCommit const& op) -> Result {
  TRI_ASSERT(op.tid.isFollowerTransactionId());
  auto trx = getTrx(op.tid);
  ADB_PROD_ASSERT(trx != nullptr)
      << "Transaction " << op.tid << " not found for operation " << op
      << " (gid " << _gid << ")";
  return trx->intermediateCommit();
}

auto DocumentStateTransactionHandler::applyOp(
    ModifiesUserTransaction auto const& op) -> Result {
  TRI_ASSERT(op.tid.isFollowerTransactionId()) << op << " " << _gid;

  auto trx = getTrx(op.tid);
  if (trx == nullptr) {
    using T = std::decay_t<decltype(op)>;
    auto accessType = std::is_same_v<T, ReplicatedOperation::Truncate>
                          ? AccessMode::Type::EXCLUSIVE
                          : AccessMode::Type::WRITE;
    TRI_ASSERT(_vocbase != nullptr) << op << " " << _gid;
    trx = _factory->createTransaction(*_vocbase, op.tid, op.shard, accessType);
    setTrx(op.tid, trx);
  }

  auto opRes = trx->apply(op);
  return _errorHandler->handleDocumentTransactionResult(opRes, op.tid);
}

auto DocumentStateTransactionHandler::applyOp(
    ReplicatedOperation::AbortAllOngoingTrx const&) -> Result {
  _transactions.getLockedGuard()->clear();
  return {};
}

auto DocumentStateTransactionHandler::applyOp(
    ReplicatedOperation::CreateShard const& op) -> Result {
  return _shardHandler->ensureShard(op.shard, op.collectionType, op.properties);
}

auto DocumentStateTransactionHandler::applyOp(
    ReplicatedOperation::ModifyShard const& op) -> Result {
  return _shardHandler->modifyShard(op.shard, op.collection, op.properties);
}

auto DocumentStateTransactionHandler::applyOp(
    ReplicatedOperation::DropShard const& op) -> Result {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // Make sure all transactions are aborted before dropping a shard.
  auto transactions = getTransactionsForShard(op.shard);
  TRI_ASSERT(transactions.empty())
      << "On follower " << _gid
      << " some transactions were not aborted before dropping shard "
      << op.shard << ": " << transactions;
#endif
  return _shardHandler->dropShard(op.shard);
}

auto DocumentStateTransactionHandler::applyOp(
    ReplicatedOperation::CreateIndex const& op) -> Result {
  return _shardHandler->ensureIndex(op.shard, op.properties,
                                    op.params.progress);
}

auto DocumentStateTransactionHandler::applyOp(
    ReplicatedOperation::DropIndex const& op) -> Result {
  return _shardHandler->dropIndex(op.shard, op.indexId);
}

auto DocumentStateTransactionHandler::applyEntry(
    ReplicatedOperation operation) noexcept -> Result {
  return applyEntry(operation.operation);
}

auto DocumentStateTransactionHandler::applyEntry(
    ReplicatedOperation::OperationType const& operation) noexcept -> Result {
  auto res = basics::catchToResult([&]() {
    return std::visit([&](auto const& op) -> Result { return applyOp(op); },
                      operation);
  });
  if (res.fail()) {
    LOG_CTX("01202", DEBUG, _loggerContext)
        << "Error occurred while applying operation " << operation << " " << res
        << ". This is not necessarily a problem. Some errors are expected to "
           "occur during leader or follower recovery.";
  }
  return res;
}
}  // namespace arangodb::replication2::replicated_state::document
