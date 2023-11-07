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

#include "Replication2/StateMachines/Document/CollectionReader.h"
#include "Replication2/StateMachines/Document/DocumentStateSnapshot.h"

#include <string_view>
#include <unordered_map>
#include <shared_mutex>

struct TRI_vocbase_t;

namespace arangodb::replication2::replicated_state::document {
struct ICollectionReader;
struct IDatabaseSnapshot;
struct IDatabaseSnapshotFactory;

/*
 * Manages snapshots on the leader.
 */
struct IDocumentStateSnapshotHandler {
  virtual ~IDocumentStateSnapshotHandler() = default;

  // Create a new snapshot
  virtual auto create(std::vector<std::shared_ptr<LogicalCollection>> shards,
                      SnapshotParams::Start const& params) noexcept
      -> ResultT<std::weak_ptr<Snapshot>> = 0;

  // Find a snapshot by id
  virtual auto find(SnapshotId const& id) noexcept
      -> ResultT<std::weak_ptr<Snapshot>> = 0;

  virtual auto abort(SnapshotId const& id) -> Result = 0;

  virtual auto finish(SnapshotId const& id) -> Result = 0;

  [[nodiscard]] virtual auto status() const -> AllSnapshotsStatus = 0;

  virtual void clear() = 0;

  virtual void giveUpOnShard(ShardID const& shardId) = 0;
};

class DocumentStateSnapshotHandler
    : public IDocumentStateSnapshotHandler,
      public std::enable_shared_from_this<DocumentStateSnapshotHandler> {
 public:
  explicit DocumentStateSnapshotHandler(
      std::unique_ptr<IDatabaseSnapshotFactory> databaseSnapshotFactory,
      cluster::RebootTracker& rebootTracker, GlobalLogIdentifier gid,
      LoggerContext loggerContext);

  auto create(std::vector<std::shared_ptr<LogicalCollection>> shards,
              SnapshotParams::Start const& params) noexcept
      -> ResultT<std::weak_ptr<Snapshot>> override;

  auto find(SnapshotId const& id) noexcept
      -> ResultT<std::weak_ptr<Snapshot>> override;

  // Abort a snapshot and remove it
  auto abort(SnapshotId const& id) noexcept -> Result override;

  // Finish a snapshot and remove it
  auto finish(SnapshotId const& id) noexcept -> Result override;

  // Clear all snapshots
  void clear() noexcept override;

  // Get the status of every snapshot
  auto status() const -> AllSnapshotsStatus override;

  // Aborts all snapshots containing a shard , so the shard can be dropped
  // afterwards
  void giveUpOnShard(ShardID const& shardId) override;

 private:
  std::unique_ptr<IDatabaseSnapshotFactory> _databaseSnapshotFactory;
  cluster::RebootTracker& _rebootTracker;
  GlobalLogIdentifier const _gid;
  LoggerContext const _loggerContext;

 private:
  struct SnapshotGuard {
    explicit SnapshotGuard() = default;

    SnapshotGuard(SnapshotGuard&&) = default;

    ~SnapshotGuard();

    std::shared_ptr<Snapshot> snapshot;
    cluster::CallbackGuard cbGuard;

    // Shortcut so we can access the snapshot directly, since we don't care to
    // access the cbGuard by itself.
    std::shared_ptr<Snapshot> operator->() const {
      TRI_ASSERT(snapshot != nullptr);
      return snapshot;
    }
  };

  // We don't need a lock on the snapshots, because the entire
  // DocumentStateSnapshotHandler is guarded by the leader.
  std::unordered_map<SnapshotId, SnapshotGuard> _snapshots;
};
}  // namespace arangodb::replication2::replicated_state::document
