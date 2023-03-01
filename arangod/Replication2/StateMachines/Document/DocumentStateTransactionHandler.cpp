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

namespace {

auto shouldIgnoreError(arangodb::OperationResult const& res) noexcept -> bool {
  auto ignoreError = [](ErrorCode code) {
    /*
     * These errors are ignored because the snapshot can be more recent than
     * the applied log entries.
     * TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED could happen during insert
     * operations.
     * TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND could happen during
     * remove operations.
     */
    return code == TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED ||
           code == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
  };

  if (res.fail() && !ignoreError(res.errorNumber())) {
    return false;
  }

  for (auto const& [code, cnt] : res.countErrorCodes) {
    if (!ignoreError(code)) {
      return false;
    }
  }
  return true;
}

auto makeResultFromOperationResult(arangodb::OperationResult const& res,
                                   arangodb::TransactionId tid)
    -> arangodb::Result {
  ErrorCode e{res.result.errorNumber()};
  std::stringstream msg;
  if (!res.countErrorCodes.empty()) {
    if (e == TRI_ERROR_NO_ERROR) {
      e = TRI_ERROR_TRANSACTION_INTERNAL;
    }
    msg << "Transaction " << tid << " error codes: ";
    for (auto const& it : res.countErrorCodes) {
      msg << it.first << ' ';
    }
    if (res.hasSlice()) {
      msg << "; Full result: " << res.slice().toJson();
    }
  }
  return arangodb::Result{e, std::move(msg).str()};
}

}  // namespace

namespace arangodb::replication2::replicated_state::document {

DocumentStateTransactionHandler::DocumentStateTransactionHandler(
    GlobalLogIdentifier gid, TRI_vocbase_t* vocbase,
    std::shared_ptr<IDocumentStateHandlersFactory> factory,
    std::shared_ptr<IDocumentStateShardHandler> shardHandler)
    : _gid(std::move(gid)),
      _vocbase(vocbase),
      _logContext(LoggerContext(Logger::REPLICATED_STATE)
                      .with<logContextKeyDatabaseName>(_gid.database)
                      .with<logContextKeyLogId>(_gid.id)),
      _factory(std::move(factory)),
      _shardHandler(std::move(shardHandler)) {
#ifndef ARANGODB_USE_GOOGLE_TESTS
  TRI_ASSERT(_vocbase != nullptr);
#endif
}

auto DocumentStateTransactionHandler::getTrx(TransactionId tid)
    -> std::shared_ptr<IDocumentStateTransaction> {
  auto it = _transactions.find(tid);
  if (it == _transactions.end()) {
    return nullptr;
  }
  return it->second;
}

void DocumentStateTransactionHandler::setTrx(
    TransactionId tid, std::shared_ptr<IDocumentStateTransaction> trx) {
  auto [_, isInserted] = _transactions.emplace(tid, std::move(trx));
  TRI_ASSERT(isInserted) << "Transaction " << tid << " already exists";
}

auto DocumentStateTransactionHandler::applyEntry(ReplicatedOperation operation)
    -> Result try {
  return std::visit(
      [this, &operation](auto&& op) -> Result {
        using T = std::decay_t<decltype(op)>;
        if constexpr (UserTransaction<T>) {
          TRI_ASSERT(op.tid.isFollowerTransactionId());

          auto trx = getTrx(op.tid);
          if constexpr (FinishesUserTransaction<T>) {
            auto res = Result{};
            ADB_PROD_ASSERT(trx != nullptr);
            if constexpr (std::is_same_v<T, ReplicatedOperation::Commit>) {
              res = trx->commit();
            } else if constexpr (std::is_same_v<T,
                                                ReplicatedOperation::Abort>) {
              res = trx->abort();
            }
            removeTransaction(op.tid);
            return res;
          } else if constexpr (std::is_same_v<
                                   T,
                                   ReplicatedOperation::IntermediateCommit>) {
            ADB_PROD_ASSERT(trx != nullptr);
            return trx->intermediateCommit();
          } else if constexpr (ModifiesUserTransaction<T>) {
            if (trx == nullptr) {
              auto accessType = std::is_same_v<T, ReplicatedOperation::Truncate>
                                    ? AccessMode::Type::EXCLUSIVE
                                    : AccessMode::Type::WRITE;
              trx = _factory->createTransaction(*_vocbase, op.tid, op.shard,
                                                accessType);
              setTrx(op.tid, trx);
            }
            auto opRes = trx->apply(operation);
            auto res = opRes.fail()
                           ? opRes.result
                           : makeResultFromOperationResult(opRes, op.tid);
            if (res.fail() && shouldIgnoreError(opRes)) {
              LOG_CTX("0da00", INFO, _logContext)
                  << "Result " << res << " ignored while applying transaction "
                  << op.tid;
              LOG_CTX("7eecc", DEBUG, _logContext)
                  << "Operation failure ignored for: " << operation;
              res = Result{};
            }
            return res;
          } else {
            static_assert(
                always_false_v<T>,
                "Unhandled operation type. This should never happen.");
          }
        } else if constexpr (std::is_same_v<
                                 T, ReplicatedOperation::AbortAllOngoingTrx>) {
          _transactions.clear();
          return Result{};
        } else if constexpr (std::is_same_v<T,
                                            ReplicatedOperation::CreateShard>) {
          auto shardRes = _shardHandler->ensureShard(op.shard, op.collection,
                                                     op.properties);
          if (shardRes.fail()) {
            return shardRes.result();
          }
          if (!shardRes.get()) {
            LOG_CTX("c98e9", INFO, _logContext)
                << "Shard " << op.shard << " already exists";
          }
          return Result{};
        } else if constexpr (std::is_same_v<T,
                                            ReplicatedOperation::DropShard>) {
          auto transactions = getTransactionsForShard(op.shard);
          TRI_ASSERT(transactions.empty())
              << "Some transactions were not aborted before dropping shard "
              << op.shard << ": " << transactions;
          return _shardHandler->dropShard(op.shard).result();
        } else {
          static_assert(always_false_v<T>,
                        "Unhandled operation type. This should never happen.");
        }
      },
      operation.operation);
} catch (basics::Exception& e) {
  return Result{e.code(), e.message()};
} catch (std::exception& e) {
  return Result{TRI_ERROR_TRANSACTION_INTERNAL, e.what()};
} catch (...) {
  LOG_CTX("01202", FATAL, _logContext)
      << "Caught unknown exception while applying operation " << operation;
  FATAL_ERROR_EXIT();
}

void DocumentStateTransactionHandler::removeTransaction(TransactionId tid) {
  _transactions.erase(tid);
}

auto DocumentStateTransactionHandler::getUnfinishedTransactions() const
    -> TransactionMap const& {
  return _transactions;
}

auto DocumentStateTransactionHandler::getTransactionsForShard(
    ShardID const& sid) -> std::vector<TransactionId> {
  std::vector<TransactionId> result;
  for (auto const& [tid, trx] : _transactions) {
    if (trx->containsShard(sid)) {
      result.emplace_back(tid);
    }
  }
  return result;
}

[[nodiscard]] auto DocumentStateTransactionHandler::validate(
    ReplicatedOperation operation) const -> Result {
  return std::visit(
      [&](auto&& op) -> Result {
        using T = std::decay_t<decltype(op)>;
        if constexpr (ModifiesUserTransaction<T>) {
          if (!_shardHandler->isShardAvailable(op.shard)) {
            return Result{TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND};
          }
        }
        return Result{};
      },
      operation.operation);
}

}  // namespace arangodb::replication2::replicated_state::document
