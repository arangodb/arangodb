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

#include "Replication2/StateMachines/Document/ActiveTransactionsQueue.h"
#include "Replication2/StateMachines/Document/DocumentCore.h"
#include "Replication2/StateMachines/Document/DocumentStateMachine.h"
#include "Replication2/StateMachines/Document/DocumentStateSnapshotHandler.h"

#include "Basics/UnshackledMutex.h"

#include <atomic>
#include <memory>

namespace arangodb::transaction {
struct IManager;
}

namespace arangodb::replication2::replicated_state::document {
struct DocumentLeaderState
    : replicated_state::IReplicatedLeaderState<DocumentState>,
      std::enable_shared_from_this<DocumentLeaderState> {
  explicit DocumentLeaderState(
      std::unique_ptr<DocumentCore> core,
      std::shared_ptr<IDocumentStateHandlersFactory> handlersFactory,
      transaction::IManager& transactionManager);

  [[nodiscard]] auto resign() && noexcept
      -> std::unique_ptr<DocumentCore> override;

  auto recoverEntries(std::unique_ptr<EntryIterator> ptr)
      -> futures::Future<Result> override;

  auto replicateOperation(velocypack::SharedSlice payload,
                          OperationType operation, TransactionId transactionId,
                          ShardID shard, ReplicationOptions opts)
      -> futures::Future<LogIndex>;

  auto createShard(ShardID shard, CollectionID collectionId,
                   velocypack::SharedSlice properties)
      -> futures::Future<Result>;
  auto dropShard(ShardID shard) -> futures::Future<Result>;
  auto modifyShard(ShardID shard) -> futures::Future<Result>;

  std::size_t getActiveTransactionsCount() const noexcept {
    return _activeTransactions.getLockedGuard()->size();
  }

  auto snapshotStart(SnapshotParams::Start const& params)
      -> ResultT<SnapshotBatch>;
  auto snapshotNext(SnapshotParams::Next const& params)
      -> ResultT<SnapshotBatch>;
  auto snapshotFinish(SnapshotParams::Finish const& params) -> Result;
  auto snapshotStatus(SnapshotId id) -> ResultT<SnapshotStatus>;
  auto allSnapshotsStatus() -> ResultT<AllSnapshotsStatus>;

  GlobalLogIdentifier const gid;
  LoggerContext const loggerContext;

 private:
  struct GuardedData {
    explicit GuardedData(std::unique_ptr<DocumentCore> core)
        : core(std::move(core)){};
    [[nodiscard]] bool didResign() const noexcept { return core == nullptr; }

    std::unique_ptr<DocumentCore> core;
  };

  template<class ResultType, class GetFunc, class ProcessFunc>
  auto executeSnapshotOperation(GetFunc getSnapshot,
                                ProcessFunc processSnapshot) -> ResultType;

  std::shared_ptr<IDocumentStateHandlersFactory> _handlersFactory;
  Guarded<std::unique_ptr<IDocumentStateSnapshotHandler>,
          basics::UnshackledMutex>
      _snapshotHandler;
  Guarded<GuardedData, basics::UnshackledMutex> _guardedData;
  Guarded<ActiveTransactionsQueue, std::mutex> _activeTransactions;
  transaction::IManager& _transactionManager;
  std::atomic_bool _isResigning;
};
}  // namespace arangodb::replication2::replicated_state::document
