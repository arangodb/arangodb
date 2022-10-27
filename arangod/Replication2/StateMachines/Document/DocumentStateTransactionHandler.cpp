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

#include "Replication2/StateMachines/Document/DocumentStateTransactionHandler.h"

#include "Basics/voc-errors.h"
#include "Logger/LogContextKeys.h"
#include "Replication2/LoggerContext.h"
#include "Replication2/StateMachines/Document/DocumentStateTransaction.h"
#include "Transaction/ReplicatedContext.h"
#include "DocumentStateHandlersFactory.h"

namespace {

auto shouldIgnoreError(arangodb::OperationResult const& res) noexcept -> bool {
  auto ignoreError = [](ErrorCode code) {
    return code == TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED;
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
    msg << "Transaction " << tid << ": ";
    for (auto const& it : res.countErrorCodes) {
      msg << it.first << ' ';
    }
  }
  return arangodb::Result{e, std::move(msg).str()};
}

}  // namespace

namespace arangodb::replication2::replicated_state::document {

DocumentStateTransactionHandler::DocumentStateTransactionHandler(
    GlobalLogIdentifier gid, std::unique_ptr<IDatabaseGuard> dbGuard,
    std::shared_ptr<IDocumentStateHandlersFactory> factory)
    : _gid(std::move(gid)),
      _dbGuard(std::move(dbGuard)),
      _factory(std::move(factory)) {}

auto DocumentStateTransactionHandler::getTrx(TransactionId tid)
    -> std::shared_ptr<IDocumentStateTransaction> {
  auto it = _transactions.find(tid);
  if (it == _transactions.end()) {
    return nullptr;
  }
  return it->second;
}

auto DocumentStateTransactionHandler::applyEntry(DocumentLogEntry doc)
    -> Result {
  if (doc.operation == OperationType::kAbortAllOngoingTrx) {
    _transactions.clear();
    return Result{};
  }

  TRI_ASSERT(doc.tid.isFollowerTransactionId());

  try {
    auto trx = ensureTransaction(doc);
    TRI_ASSERT(trx != nullptr);
    switch (doc.operation) {
      case OperationType::kInsert:
      case OperationType::kUpdate:
      case OperationType::kReplace:
      case OperationType::kRemove:
      case OperationType::kTruncate: {
        auto opRes = trx->apply(doc);
        auto res = opRes.fail() ? opRes.result
                                : makeResultFromOperationResult(opRes, doc.tid);
        if (res.fail() && shouldIgnoreError(opRes)) {
          LoggerContext logContext =
              LoggerContext(Logger::REPLICATED_STATE)
                  .with<logContextKeyDatabaseName>(_gid.database)
                  .with<logContextKeyLogId>(_gid.id);

          LOG_CTX("0da00", INFO, logContext)
              << "Result ignored while applying transaction " << doc.tid
              << " with operation " << int(doc.operation) << " on shard "
              << doc.shardId << ": " << res;
          return Result{};
        }
        return res;
      }
      case OperationType::kCommit: {
        auto res = trx->commit();
        removeTransaction(doc.tid);
        return res;
      }
      case OperationType::kAbort: {
        auto res = trx->abort();
        removeTransaction(doc.tid);
        return res;
      }
      case OperationType::kAbortAllOngoingTrx:
        TRI_ASSERT(false);  // should never happen as it should be handled above
      case OperationType::kIntermediateCommit:
        return trx->intermediateCommit();
      default:
        THROW_ARANGO_EXCEPTION(TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION);
    }
  } catch (basics::Exception& e) {
    return Result{e.code(), e.message()};
  } catch (std::exception& e) {
    return Result{TRI_ERROR_TRANSACTION_INTERNAL, e.what()};
  }
}

auto DocumentStateTransactionHandler::ensureTransaction(
    DocumentLogEntry const& doc) -> std::shared_ptr<IDocumentStateTransaction> {
  TRI_ASSERT(doc.operation != OperationType::kAbortAllOngoingTrx);

  auto tid = doc.tid;
  auto trx = getTrx(tid);
  if (trx != nullptr) {
    return trx;
  }

  trx = _factory->createTransaction(doc, *_dbGuard);
  _transactions.emplace(tid, trx);
  return trx;
}

void DocumentStateTransactionHandler::removeTransaction(TransactionId tid) {
  _transactions.erase(tid);
}

auto DocumentStateTransactionHandler::getUnfinishedTransactions() const
    -> TransactionMap const& {
  return _transactions;
}

LogIndex ActiveTransactionsQueue::getReleaseIndex() const {
  if (_logIndices.empty()) {
    return LogIndex{0};
  }
  return _logIndices.front().first.saturatedDecrement(1);
}

/**
 * @brief Remove a transaction from the active transactions map and update the
 * log indices list.
 * @return true if the transaction was found and removed, false otherwise
 */
bool ActiveTransactionsQueue::erase(TransactionId const& tid) {
  auto it = _transactions.find(tid);
  if (it == std::end(_transactions)) {
    return false;
  }

  auto deactivateIdx =
      std::lower_bound(std::begin(_logIndices), std::end(_logIndices),
                       std::make_pair(it->second, Status::ACTIVE));
  TRI_ASSERT(deactivateIdx != std::end(_logIndices) &&
             deactivateIdx->first == it->second);
  deactivateIdx->second = Status::INACTIVE;
  _transactions.erase(it);

  popInactive();

  return true;
}

/**
 * @brief Try to insert an entry into the active transactions map. If the entry
 * is new, mark the log index as active.
 */
void ActiveTransactionsQueue::emplace(TransactionId tid, LogIndex index) {
  if (_transactions.try_emplace(tid, index).second) {
    _logIndices.emplace_back(index, Status::ACTIVE);
  }
  popInactive();
}

/**
 * We should not leave the deque empty, even if the last transaction is
 * inactive. This ensures that we always have a release index to report.
 */
void ActiveTransactionsQueue::popInactive() {
  while (_logIndices.size() > 1 && _logIndices.front().second == INACTIVE) {
    _logIndices.pop_front();
  }
}

std::size_t ActiveTransactionsQueue::size() const noexcept {
  return _transactions.size();
}

auto ActiveTransactionsQueue::getTransactions() const
    -> std::unordered_map<TransactionId, LogIndex> const& {
  return _transactions;
}

void ActiveTransactionsQueue::clear() {
  _transactions.clear();
  _logIndices.clear();
}

}  // namespace arangodb::replication2::replicated_state::document
