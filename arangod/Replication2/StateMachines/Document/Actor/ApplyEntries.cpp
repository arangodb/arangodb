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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#include "ApplyEntries.h"

#include <memory>
#include <type_traits>

#include "Actor/LocalActorPID.h"
#include "Actor/LocalRuntime.h"

#include "Assertions/ProdAssert.h"

#include "Replication2/StateMachines/Document/Actor/Transaction.h"
#include "Replication2/StateMachines/Document/DocumentStateShardHandler.h"
#include "Replication2/StateMachines/Document/ReplicatedOperation.h"
#include "Transaction/Methods.h"
#include "Transaction/OperationOrigin.h"

namespace arangodb::replication2::replicated_state::document::actor {

using namespace arangodb::actor;

void ApplyEntriesState::setBatch(
    std::unique_ptr<DocumentFollowerState::EntryIterator> entries,
    futures::Promise<Result> promise) {
  ADB_PROD_ASSERT(_batch == nullptr);
  _batch = std::make_unique<Batch>(std::move(entries), std::move(promise));
}

void ApplyEntriesState::resign() {
  if (_batch) {
    _batch->promise.setValue(
        Result{TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED});
    _batch.reset();
  }
}

void ApplyEntriesState::resolveBatch(Result result) {
  _batch->promise.setValue(result);
  if (result.ok() && _batch->lastIndex.has_value()) {
    _state._guardedData.doUnderLock([&](auto& data) {
      auto releaseIdx = data.activeTransactions.getReleaseIndex().value_or(
          _batch->lastIndex.value());
      releaseIndex(releaseIdx);
    });
  }
  _batch.reset();
}

auto ApplyEntriesState::processEntry(DataDefinition auto& op, LogIndex index)
    -> ResultT<ProcessResult> {
  if (not _pendingTransactions.empty()) {
    return ProcessResult::kWaitForPendingTrx;
  }
  return _state._guardedData.doUnderLock(
      [&](auto& data) -> ResultT<ProcessResult> {
        auto res = applyDataDefinitionEntry(data, op);
        if (res.fail()) {
          return res;
        }
        _batch->lastIndex = index;
        return ProcessResult::kContinue;
      });
}

auto ApplyEntriesState::processEntry(
    ReplicatedOperation::AbortAllOngoingTrx& op, LogIndex index)
    -> ResultT<ProcessResult> {
  if (not _pendingTransactions.empty()) {
    return ProcessResult::kWaitForPendingTrx;
  }
  auto originalRes = handlers.transactionHandler->applyEntry(op);
  auto res = handlers.errorHandler->handleOpResult(op, originalRes);
  if (res.fail()) {
    return res;
  }

  _batch->lastIndex = index;
  return _state._guardedData.doUnderLock([&](auto& data) {
    data.activeTransactions.clear();
    return ProcessResult::kContinue;
  });
}

auto ApplyEntriesState::processEntry(UserTransaction auto& op, LogIndex index)
    -> ResultT<ProcessResult> {
  using OpType = std::remove_cvref_t<decltype(op)>;
  LocalActorPID pid;
  auto it = _activeTransactions.find(op.tid);
  if (it != _activeTransactions.end()) {
    pid = it->second;
  } else {
    pid = _state._runtime->template spawn<actor::TransactionActor>(
        std::make_unique<actor::TransactionState>(_state));
    LOG_CTX("8a74c", DEBUG, loggerContext)
        << "spawned transaction actor " << pid.id << " for trx " << op.tid;
    _state._runtime->monitorActor(myPid, pid);
    _activeTransactions.emplace(op.tid, pid);
  }

  if (not beforeApplyEntry(op, index)) {
    // if beforeApplyEntry returns false, we can simply skip this entry
    // this is not an error!
    return ResultT<ProcessResult>::success(ProcessResult::kContinue);
  }

  constexpr bool isIntermediateCommit =
      std::is_same_v<OpType, ReplicatedOperation::IntermediateCommit>;
  if constexpr (FinishesUserTransaction<OpType> || isIntermediateCommit) {
    // this is either a commit or an abort - we remove the transaction from the
    // active transaction map and instead insert it in the pending transactions,
    // so other operations can wait for it to finish
    // Note: we handle intermediate commits the same way as regular commits,
    // because subsequent operations that belong to the same transaction will
    // simply start a new transaction actor with the same transaction id
    _activeTransactions.erase(op.tid);
    _pendingTransactions.emplace(
        pid, TransactionInfo{.tid = op.tid,
                             .intermediateCommit = isIntermediateCommit});
  }

  if constexpr (FinishesUserTransaction<OpType>) {
    _batch->lastIndex = index;
  }

  _state._runtime->dispatch<message::TransactionMessages>(
      pid, pid, message::ProcessEntry{.op = std::move(op), .index = index});

  if constexpr (FinishesUserTransaction<OpType> ||
                std::is_same_v<OpType,
                               ReplicatedOperation::IntermediateCommit>) {
    // we need to wait for the transaction to be committed before we can
    // continue
    // TODO - once we have proper dependency tracking in place this can be
    // relaxed
    return ResultT<ProcessResult>::success(
        ProcessResult::kMoveToNextEntryAndWaitForPendingTrx);
  } else {
    return ResultT<ProcessResult>::success(ProcessResult::kContinue);
  }
}

auto ApplyEntriesState::applyDataDefinitionEntry(
    DocumentFollowerState::GuardedData& data,
    ReplicatedOperation::DropShard const& op) -> Result {
  // We first have to abort all transactions for this shard.
  // This stunt may seem unnecessary, as the leader counterpart takes care of
  // this by replicating the abort operations. However, because we're currently
  // replicating the "DropShard" operation first, "Abort" operations come later.
  // Hence, we need to abort transactions manually for now.
  for (auto const& tid :
       handlers.transactionHandler->getTransactionsForShard(op.shard)) {
    auto abortRes = handlers.transactionHandler->applyEntry(
        ReplicatedOperation::buildAbortOperation(tid));
    if (abortRes.fail()) {
      LOG_CTX("aa36c", INFO, loggerContext)
          << "Failed to abort transaction " << tid << " for shard " << op.shard
          << " before dropping the shard: " << abortRes.errorMessage();
      return abortRes;
    }
    data.activeTransactions.markAsInactive(tid);
  }
  return applyEntryAndReleaseIndex(data, op);
}

auto ApplyEntriesState::applyDataDefinitionEntry(
    DocumentFollowerState::GuardedData& data,
    ReplicatedOperation::ModifyShard const& op) -> Result {
  // Note that locking the shard is not necessary on the follower.
  // However, we still do it for safety reasons.
  auto origin =
      transaction::OperationOriginREST{"follower collection properties update"};
  auto trxLock = handlers.shardHandler->lockShard(
      op.shard, AccessMode::Type::EXCLUSIVE, std::move(origin));
  if (trxLock.fail()) {
    auto res = handlers.errorHandler->handleOpResult(op, trxLock.result());

    // If the shard was not found, we can ignore this operation and release it.
    if (res.ok()) {
      return {};
    }

    return res;
  }
  return applyEntryAndReleaseIndex(data, op);
}

template<class T>
requires IsAnyOf<T, ReplicatedOperation::CreateShard,
                 ReplicatedOperation::CreateIndex,
                 ReplicatedOperation::DropIndex>
auto ApplyEntriesState::applyDataDefinitionEntry(
    DocumentFollowerState::GuardedData& data, T const& op) -> Result {
  return applyEntryAndReleaseIndex(data, op);
}

template<class T>
auto ApplyEntriesState::applyEntryAndReleaseIndex(
    DocumentFollowerState::GuardedData& data, T const& op) -> Result {
  auto originalRes = handlers.transactionHandler->applyEntry(op);
  auto res = handlers.errorHandler->handleOpResult(op, originalRes);
  if (res.fail()) {
    return res;
  }
  return {};
}

auto ApplyEntriesState::beforeApplyEntry(ModifiesUserTransaction auto const& op,
                                         LogIndex index) -> bool {
  return _state._guardedData.doUnderLock([&](auto& data) {
    data.activeTransactions.markAsActive(op.tid, index);
    return true;
  });
}

auto ApplyEntriesState::beforeApplyEntry(
    ReplicatedOperation::IntermediateCommit const& op, LogIndex) -> bool {
  return _state._guardedData.doUnderLock([&](auto& data) {
    if (!data.activeTransactions.getTransactions().contains(op.tid)) {
      LOG_CTX("b41dc", INFO, data.core->loggerContext)
          << "will not apply intermediate commit for transaction " << op.tid
          << " because it is not active";
      return false;
    }
    return true;
  });
}

auto ApplyEntriesState::beforeApplyEntry(FinishesUserTransaction auto const& op,
                                         LogIndex index) -> bool {
  return _state._guardedData.doUnderLock([&](auto& data) {
    if (!data.activeTransactions.getTransactions().contains(op.tid)) {
      // Single commit/abort operations are possible.
      LOG_CTX("cf7ea", INFO, data.core->loggerContext)
          << "will not finish transaction " << op.tid
          << " because it is not active";
      return false;
    }
    return true;
  });
}

void ApplyEntriesState::releaseIndex(std::optional<LogIndex> index) {
  if (index.has_value()) {
    // The follower might have resigned, so we need to be careful when
    // accessing the stream.
    auto releaseRes = basics::catchVoidToResult([&] {
      // TODO - is this getStream call actually safe?
      auto const& stream = _state.getStream();
      stream->release(index.value());
    });
    if (releaseRes.fail()) {
      LOG_CTX("10f07", ERR, loggerContext)
          << "Failed to get stream! " << releaseRes;
    }
  }
}

void ApplyEntriesState::continueBatch() {
  ADB_PROD_ASSERT(_batch != nullptr);
  ADB_PROD_ASSERT(_pendingTransactions.empty());
  if (not _batch->_currentEntry.has_value()) {
    resolveBatch(Result{});
    return;
  }

  // TODO - exception handling
  do {
    if (_state._resigning) {
      // We have not officially resigned yet, but we are about to. So,
      // we can just stop here.
      resolveBatch(
          Result{TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED});
      _state._runtime->finishActor(myPid, ExitReason::kFinished);
      return;
    }

    auto& e = _batch->_currentEntry.value();
    LogIndex index = e.first;
    auto& doc = e.second;
    auto res = std::visit([&](auto&& op) { return processEntry(op, index); },
                          doc.getInnerOperation());

    if (res.fail()) {
      TRI_ASSERT(handlers.errorHandler
                     ->handleOpResult(doc.getInnerOperation(), res.result())
                     .fail())
          << res.result() << " should have been already handled for operation "
          << doc.getInnerOperation() << " during applyEntries of follower "
          << _state.gid;
      LOG_CTX("0aa2e", FATAL, loggerContext)
          << "failed to apply entry " << doc
          << " on follower: " << res.result();
      TRI_ASSERT(false) << res.result();
      FATAL_ERROR_EXIT();
    }

    if (res.get() == ProcessResult::kWaitForPendingTrx) {
      ADB_PROD_ASSERT(not _pendingTransactions.empty());
      // the current entry requires all pending transactions to have finished,
      // so we return here and wait for the transactions's finish message before
      // we continue
      return;
    }

    _batch->_currentEntry = _batch->entries->next();
    if (res.get() == ProcessResult::kMoveToNextEntryAndWaitForPendingTrx) {
      // we successfully processed the last entry and moved on, but the last
      // entry indicated that we have to wait for pending transactions to finish
      // before processing the next entry, so we return here and wait for the
      // transactions's finish message before we continue
      ADB_PROD_ASSERT(not _pendingTransactions.empty());
      return;
    }
  } while (_batch->_currentEntry.has_value());

  if (_pendingTransactions.empty()) {
    resolveBatch(Result{});
  }
  // we have processed all entries, but there are still pending transactions
  // that we need to wait for, before we can resolve the batch
}

}  // namespace arangodb::replication2::replicated_state::document::actor
