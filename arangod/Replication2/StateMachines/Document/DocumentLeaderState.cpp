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
#include "Replication2/StateMachines/Document/DocumentStateMachine.h"
#include "Replication2/StateMachines/Document/DocumentStateHandlersFactory.h"
#include "Replication2/StateMachines/Document/DocumentStateTransactionHandler.h"

#include <Futures/Future.h>
#include <Logger/LogContextKeys.h>

using namespace arangodb::replication2::replicated_state::document;

DocumentLeaderState::DocumentLeaderState(
    std::unique_ptr<DocumentCore> core,
    std::shared_ptr<IDocumentStateHandlersFactory> handlersFactory)
    : loggerContext(
          core->loggerContext.with<logContextKeyStateComponent>("LeaderState")),
      shardId(core->getShardId()),
      gid(core->getGid()),
      _handlersFactory(std::move(handlersFactory)),
      _guardedData(std::move(core)) {}

auto DocumentLeaderState::resign() && noexcept
    -> std::unique_ptr<DocumentCore> {
  return _guardedData.doUnderLock([](auto& data) {
    if (data.didResign()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_NOT_LEADER);
    }
    return std::move(data.core);
  });
}

auto DocumentLeaderState::recoverEntries(std::unique_ptr<EntryIterator> ptr)
    -> futures::Future<Result> {
  auto transactionHandler = _handlersFactory->createTransactionHandler(gid);

  while (auto entry = ptr->next()) {
    auto doc = entry->second;
    auto res = transactionHandler->applyEntry(doc);
    if (res.fail()) {
      return res;
    }
  }

  auto doc = DocumentLogEntry{std::string(shardId),
                              OperationType::kAbortAllOngoingTrx,
                              {},
                              TransactionId{0}};
  auto stream = getStream();
  stream->insert(doc);

  // TODO Add a tombstone to the TransactionManager
  return {TRI_ERROR_NO_ERROR};
}

auto DocumentLeaderState::replicateOperation(velocypack::SharedSlice payload,
                                             OperationType operation,
                                             TransactionId transactionId,
                                             ReplicationOptions opts)
    -> futures::Future<LogIndex> {
  auto entry = DocumentLogEntry{std::string(shardId), operation,
                                std::move(payload), transactionId};
  auto stream = getStream();
  auto idx = stream->insert(entry);

  if (opts.waitForCommit) {
    return stream->waitFor(idx).thenValue(
        [idx](auto&& result) { return futures::Future<LogIndex>{idx}; });
  }

  return futures::Future<LogIndex>{idx};
}

#include "Replication2/ReplicatedState/ReplicatedState.tpp"
