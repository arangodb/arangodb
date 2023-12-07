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
#include "Replication2/StateMachines/Document/DocumentStateErrorHandler.h"
#include "Replication2/StateMachines/Document/DocumentStateTransactionHandler.h"
#include "Replication2/StateMachines/Document/DocumentStateSnapshot.h"
#include "Replication2/StateMachines/Document/ReplicatedOperation.h"

#include "Basics/UnshackledMutex.h"

#include <function2.hpp>

namespace arangodb::replication2::replicated_state::document {

struct IDocumentStateLeaderInterface;
struct IDocumentStateNetworkHandler;
struct SnapshotBatch;

struct DocumentFollowerState
    : replicated_state::IReplicatedFollowerState<DocumentState>,
      std::enable_shared_from_this<DocumentFollowerState> {
  explicit DocumentFollowerState(
      std::unique_ptr<DocumentCore> core,
      std::shared_ptr<IDocumentStateHandlersFactory> const& handlersFactory);

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

 private:
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

 private:
  struct GuardedData {
    explicit GuardedData(
        std::unique_ptr<DocumentCore> core,
        std::shared_ptr<IDocumentStateHandlersFactory> const& handlersFactory,
        LoggerContext const& loggerContext,
        std::shared_ptr<IDocumentStateErrorHandler> errorHandler);

    LoggerContext const& loggerContext;
    std::shared_ptr<IDocumentStateErrorHandler> errorHandler;

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
    auto applyEntry(ReplicatedOperation::CreateIndex const&, LogIndex)
        -> ResultT<std::optional<LogIndex>>;
    auto applyEntry(ReplicatedOperation::DropIndex const&, LogIndex)
        -> ResultT<std::optional<LogIndex>>;

    template<class T>
    auto applyAndRelease(
        T const& op, std::optional<LogIndex> index = std::nullopt,
        std::optional<fu2::unique_function<void(Result&&)>> fun = std::nullopt)
        -> ResultT<std::optional<LogIndex>>;

    std::unique_ptr<DocumentCore> core;
    std::uint64_t currentSnapshotVersion;
    std::shared_ptr<IDocumentStateShardHandler> shardHandler;
    std::shared_ptr<IDocumentStateTransactionHandler> transactionHandler;
    ActiveTransactionsQueue activeTransactions;
  };

  std::shared_ptr<IDocumentStateNetworkHandler> _networkHandler;
  std::shared_ptr<IDocumentStateShardHandler> _shardHandler;
  std::shared_ptr<IDocumentStateErrorHandler> _errorHandler;
  Guarded<GuardedData, basics::UnshackledMutex> _guardedData;

  std::atomic<bool> _resigning{false};  // Allows for a quicker shutdown of the
                                        // state machine upon resigning
};

}  // namespace arangodb::replication2::replicated_state::document
