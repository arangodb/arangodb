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
#include "Replication2/StateMachines/Document/DocumentStateTransactionHandler.h"
#include "Replication2/StateMachines/Document/DocumentStateSnapshot.h"
#include "Replication2/StateMachines/Document/ReplicatedOperation.h"

#include "Basics/UnshackledMutex.h"

namespace arangodb::replication2::replicated_state::document {

struct IDocumentStateLeaderInterface;
struct IDocumentStateNetworkHandler;
struct SnapshotConfig;
struct SnapshotBatch;

struct DocumentFollowerState
    : replicated_state::IReplicatedFollowerState<DocumentState>,
      std::enable_shared_from_this<DocumentFollowerState> {
  explicit DocumentFollowerState(
      std::unique_ptr<DocumentCore> core,
      std::shared_ptr<IDocumentStateHandlersFactory> const& handlersFactory);
  ~DocumentFollowerState() override;

  LoggerContext const loggerContext;

  auto getAssociatedShardList() const -> std::vector<ShardID>;

 protected:
  [[nodiscard]] auto resign() && noexcept
      -> std::unique_ptr<DocumentCore> override;
  auto acquireSnapshot(ParticipantId const& destination) noexcept
      -> futures::Future<Result> override;
  auto applyEntries(std::unique_ptr<EntryIterator> ptr) noexcept
      -> futures::Future<Result> override;

 private:
  static auto populateLocalShard(
      ShardID shardId, velocypack::SharedSlice slice,
      std::shared_ptr<IDocumentStateTransactionHandler> const&
          transactionHandler) -> Result;

  struct SnapshotTransferResult {
    Result res{};
    bool reportFailure{};
    std::optional<SnapshotId> snapshotId{};
  };

  auto handleSnapshotTransfer(
      std::shared_ptr<IDocumentStateLeaderInterface> leader,
      std::uint64_t snapshotVersion,
      futures::Future<ResultT<SnapshotConfig>>&& snapshotFuture) noexcept
      -> futures::Future<SnapshotTransferResult>;
  auto handleSnapshotTransfer(
      SnapshotId shapshotId,
      std::shared_ptr<IDocumentStateLeaderInterface> leader,
      std::uint64_t snapshotVersion, std::optional<ShardID> currentShard,
      futures::Future<ResultT<SnapshotBatch>>&& snapshotFuture) noexcept
      -> futures::Future<SnapshotTransferResult>;

 private:
  struct GuardedData {
    explicit GuardedData(
        std::unique_ptr<DocumentCore> core,
        std::shared_ptr<IDocumentStateHandlersFactory> const& handlersFactory);

    [[nodiscard]] bool didResign() const noexcept { return core == nullptr; }

    auto applyEntry(ModifiesUserTransaction auto const&, LogIndex)
        -> ResultT<std::optional<LogIndex>>;
    auto applyEntry(ReplicatedOperation::IntermediateCommit const&, LogIndex)
        -> ResultT<std::optional<LogIndex>>;
    auto applyEntry(FinishesUserTransaction auto const&, LogIndex)
        -> ResultT<std::optional<LogIndex>>;
    auto applyEntry(ReplicatedOperation::AbortAllOngoingTrx const&, LogIndex)
        -> ResultT<std::optional<LogIndex>>;
    auto applyEntry(ReplicatedOperation::ModifyShard const&, LogIndex)
        -> ResultT<std::optional<LogIndex>>;
    auto applyEntry(ReplicatedOperation::DropShard const&, LogIndex)
        -> ResultT<std::optional<LogIndex>>;
    auto applyEntry(ReplicatedOperation::CreateShard const&, LogIndex)
        -> ResultT<std::optional<LogIndex>>;

    std::unique_ptr<DocumentCore> core;
    std::uint64_t currentSnapshotVersion;
    std::shared_ptr<IDocumentStateTransactionHandler> transactionHandler;
    ActiveTransactionsQueue activeTransactions;
  };

  std::shared_ptr<IDocumentStateNetworkHandler> _networkHandler;
  Guarded<GuardedData, basics::UnshackledMutex> _guardedData;
};

}  // namespace arangodb::replication2::replicated_state::document
