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

#include "Replication2/StateMachines/Document/DocumentLeaderState.h"

#include "Replication2/StateMachines/Document/DocumentFollowerState.h"
#include "Replication2/StateMachines/Document/DocumentLogEntry.h"
#include "Replication2/StateMachines/Document/DocumentStateMachine.h"
#include "Replication2/StateMachines/Document/DocumentStateHandlersFactory.h"
#include "Replication2/StateMachines/Document/DocumentStateTransactionHandler.h"
#include "Transaction/Manager.h"

#include <Futures/Future.h>
#include <Logger/LogContextKeys.h>

namespace arangodb::replication2::replicated_state::document {

DocumentLeaderState::DocumentLeaderState(
    std::unique_ptr<DocumentCore> core,
    std::shared_ptr<IDocumentStateHandlersFactory> handlersFactory,
    transaction::IManager& transactionManager)
    : loggerContext(
          core->loggerContext.with<logContextKeyStateComponent>("LeaderState")),
      shardId(core->getShardId()),
      gid(core->getGid()),
      _handlersFactory(std::move(handlersFactory)),
      _guardedData(std::move(core)),
      _transactionManager(transactionManager) {}

auto DocumentLeaderState::resign() && noexcept
    -> std::unique_ptr<DocumentCore> {
  auto transactions = getActiveTransactions();
  for (auto trx : transactions) {
    try {
      _transactionManager.abortManagedTrx(trx, gid.database);
    } catch (...) {
      LOG_CTX("7341f", WARN, loggerContext)
          << "failed to abort active transaction " << gid << " during resign";
    }
  }

  return _guardedData.doUnderLock([](auto& data) {
    if (data.didResign()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_NOT_LEADER);
    }
    return std::move(data.core);
  });
}

auto DocumentLeaderState::recoverEntries(std::unique_ptr<EntryIterator> ptr)
    -> futures::Future<Result> {
  return _guardedData.doUnderLock([self = shared_from_this(),
                                   ptr = std::move(ptr)](
                                      auto& data) -> futures::Future<Result> {
    if (data.didResign()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_NOT_LEADER);
    }

    auto transactionHandler =
        self->_handlersFactory->createTransactionHandler(self->gid);
    if (transactionHandler == nullptr) {
      // TODO this is a temporary fix, see CINFRA-588
      return Result{
          TRI_ERROR_ARANGO_DATABASE_NOT_FOUND,
          fmt::format("Transaction handler is missing from DocumentLeaderState "
                      "during recoverEntries "
                      "{}! This happens if the vocbase cannot be found during "
                      "DocumentState construction.",
                      to_string(self->gid))};
    }

    while (auto entry = ptr->next()) {
      auto doc = entry->second;
      auto res = transactionHandler->applyEntry(doc);
      if (res.fail()) {
        return res;
      }
    }

    auto doc = DocumentLogEntry{std::string(self->shardId),
                                OperationType::kAbortAllOngoingTrx,
                                {},
                                TransactionId{0}};
    auto stream = self->getStream();
    stream->insert(doc);

    for (auto& [tid, trx] : transactionHandler->getActiveTransactions()) {
      try {
        // the log entries contain follower ids, which is fine since during
        // recovery we apply the entries like a follower, but we have to
        // register tombstones in the trx managers for the leader trx id.
        self->_transactionManager.abortManagedTrx(tid.asLeaderTransactionId(),
                                                  self->gid.database);
      } catch (...) {
        LOG_CTX("894f1", WARN, self->loggerContext)
            << "failed to abort active transaction " << self->gid
            << " during recovery";
      }
    }

    return {TRI_ERROR_NO_ERROR};
  });
}

auto DocumentLeaderState::replicateOperation(velocypack::SharedSlice payload,
                                             OperationType operation,
                                             TransactionId transactionId,
                                             ReplicationOptions opts)
    -> futures::Future<LogIndex> {
  // we replicate operations using follower trx ids, but locally we track the
  // actual trx ids
  auto entry =
      DocumentLogEntry{std::string(shardId), operation, std::move(payload),
                       transactionId.asFollowerTransactionId()};

  TRI_ASSERT(operation != OperationType::kAbortAllOngoingTrx);
  if (operation == OperationType::kCommit ||
      operation == OperationType::kAbort) {
    if (_activeTransactions.getLockedGuard()->erase(transactionId) == 0) {
      // we have not replicated anything for a transaction with this id, so
      // there is no need to replicate the abort/commit operation
      return LogIndex{};  // TODO - can we do this differently instead of
                          // returning a dummy index/
    }
  } else {
    _activeTransactions.getLockedGuard()->emplace(transactionId);
  }
  auto stream = getStream();
  auto idx = stream->insert(entry);

  if (opts.waitForCommit) {
    return stream->waitFor(idx).thenValue(
        [idx](auto&& result) { return futures::Future<LogIndex>{idx}; });
  }

  return futures::Future<LogIndex>{idx};
}

std::unordered_set<TransactionId> DocumentLeaderState::getActiveTransactions()
    const {
  return _activeTransactions.getLockedGuard().get();
}

}  // namespace arangodb::replication2::replicated_state::document

#include "Replication2/ReplicatedState/ReplicatedState.tpp"
