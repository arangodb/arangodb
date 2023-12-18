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

#include "Replication2/StateMachines/Document/DocumentCore.h"
#include "Replication2/StateMachines/Document/DocumentStateErrorHandler.h"
#include "Replication2/StateMachines/Document/DocumentStateTransactionHandler.h"
#include "Replication2/StateMachines/Document/DocumentStateSnapshot.h"
#include "Replication2/StateMachines/Document/ReplicatedOperation.h"

#include "Actor/LocalActorPID.h"
#include "Basics/UnshackledMutex.h"

#include <function2.hpp>

namespace arangodb::actor {
struct LocalRuntime;
}

namespace arangodb::replication2::replicated_state::document {

struct IDocumentStateLeaderInterface;
struct IDocumentStateNetworkHandler;
struct SnapshotBatch;

struct Handlers {
  Handlers(
      std::shared_ptr<IDocumentStateHandlersFactory> const& handlersFactory,
      TRI_vocbase_t& vocbase, GlobalLogIdentifier gid);
  std::shared_ptr<IDocumentStateShardHandler> const shardHandler;
  std::shared_ptr<IDocumentStateTransactionHandler> const transactionHandler;
  std::shared_ptr<IDocumentStateErrorHandler> const errorHandler;
};

struct DocumentFollowerState
    : replicated_state::IReplicatedFollowerState<DocumentState>,
      std::enable_shared_from_this<DocumentFollowerState> {
  explicit DocumentFollowerState(
      std::unique_ptr<DocumentCore> core,
      std::shared_ptr<IDocumentStateHandlersFactory> const& handlersFactory,
      std::shared_ptr<IScheduler> scheduler);

  ~DocumentFollowerState() override;

  GlobalLogIdentifier const gid;
  LoggerContext const loggerContext;

  auto getAssociatedShardList() const -> std::vector<ShardID>;

 protected:
  [[nodiscard]] auto resign() && noexcept
      -> std::unique_ptr<DocumentCore> override;

  auto acquireSnapshot(ParticipantId const& destination) noexcept
      -> futures::Future<Result> override;

  auto applyEntries(std::unique_ptr<EntryIterator> ptr) noexcept
      -> futures::Future<Result> override;

  using replicated_state::IReplicatedFollowerState<DocumentState>::getStream;

 private:
  void shutdownRuntime() noexcept;

  struct SnapshotTransferResult {
    Result res{};
    bool reportFailure{};
    std::optional<SnapshotId> snapshotId{};
  };

  auto handleSnapshotTransfer(
      std::optional<SnapshotId> snapshotId,
      std::shared_ptr<IDocumentStateLeaderInterface> leader,
      std::uint64_t snapshotVersion,
      futures::Future<ResultT<SnapshotBatch>>&& snapshotFuture) noexcept
      -> futures::Future<SnapshotTransferResult>;

  struct GuardedData {
    explicit GuardedData(std::unique_ptr<DocumentCore> core,
                         LoggerContext const& loggerContext);

    LoggerContext const& loggerContext;

    [[nodiscard]] bool didResign() const noexcept { return core == nullptr; }

    std::unique_ptr<DocumentCore> core;
    std::uint64_t currentSnapshotVersion;
  };

  std::shared_ptr<IDocumentStateNetworkHandler> const _networkHandler;
  Handlers const _handlers;
  Guarded<GuardedData, basics::UnshackledMutex> _guardedData;

  std::atomic<bool> _resigning{false};  // Allows for a quicker shutdown of
                                        // the state machine upon resigning

  std::shared_ptr<actor::LocalRuntime> _runtime;
  actor::LocalActorPID _applyEntriesActor;
};

}  // namespace arangodb::replication2::replicated_state::document
