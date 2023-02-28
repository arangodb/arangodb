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

#include "Basics/UnshackledMutex.h"

namespace arangodb::replication2::replicated_state::document {

struct IDocumentStateLeaderInterface;
struct IDocumentStateNetworkHandler;
struct IDocumentStateTransactionHandler;
enum class OperationType;
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

 protected:
  [[nodiscard]] auto resign() && noexcept
      -> std::unique_ptr<DocumentCore> override;
  auto acquireSnapshot(ParticipantId const& destination, LogIndex) noexcept
      -> futures::Future<Result> override;
  auto applyEntries(std::unique_ptr<EntryIterator> ptr) noexcept
      -> futures::Future<Result> override;

 private:
  static auto populateLocalShard(
      ShardID shardId, velocypack::SharedSlice slice,
      std::shared_ptr<IDocumentStateTransactionHandler> const&
          transactionHandler) -> Result;
  auto handleSnapshotTransfer(
      std::shared_ptr<IDocumentStateLeaderInterface> leader,
      LogIndex waitForIndex, std::uint64_t snapshotVersion,
      futures::Future<ResultT<SnapshotConfig>>&& snapshotFuture) noexcept
      -> futures::Future<Result>;
  auto handleSnapshotTransfer(
      std::shared_ptr<IDocumentStateLeaderInterface> leader,
      LogIndex waitForIndex, std::uint64_t snapshotVersion,
      futures::Future<ResultT<SnapshotBatch>>&& snapshotFuture) noexcept
      -> futures::Future<Result>;

 private:
  struct GuardedData {
    explicit GuardedData(std::unique_ptr<DocumentCore> core)
        : core(std::move(core)), currentSnapshotVersion{0} {};
    [[nodiscard]] bool didResign() const noexcept { return core == nullptr; }

    std::unique_ptr<DocumentCore> core;
    std::uint64_t currentSnapshotVersion;
  };

  std::shared_ptr<IDocumentStateNetworkHandler> _networkHandler;
  Guarded<GuardedData, basics::UnshackledMutex> _guardedData;
  ActiveTransactionsQueue _activeTransactions;
};

}  // namespace arangodb::replication2::replicated_state::document
