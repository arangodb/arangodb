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

#include "Basics/Guarded.h"
#include "Basics/UnshackledMutex.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/StateMachines/Document/ShardProperties.h"

#include <shared_mutex>

struct TRI_vocbase_t;

namespace arangodb::replication2::replicated_state::document {

struct IMaintenanceActionExecutor;

struct IDocumentStateShardHandler {
  virtual ~IDocumentStateShardHandler() = default;
  virtual auto ensureShard(ShardID shard, CollectionID collection,
                           std::shared_ptr<VPackBuilder> properties)
      -> ResultT<bool> = 0;
  virtual auto modifyShard(ShardID shard, CollectionID collection,
                           std::shared_ptr<VPackBuilder> properties,
                           std::string followersToDrop) -> ResultT<bool> = 0;
  virtual auto dropShard(ShardID const& shard) -> ResultT<bool> = 0;
  virtual auto dropAllShards() -> Result = 0;
  virtual auto isShardAvailable(ShardID const& shard) -> bool = 0;
  virtual auto getShardMap() -> ShardMap = 0;
};

class DocumentStateShardHandler : public IDocumentStateShardHandler {
#ifdef ARANGODB_USE_GOOGLE_TESTS
 public:
  explicit DocumentStateShardHandler(
      GlobalLogIdentifier gid,
      std::shared_ptr<IMaintenanceActionExecutor> maintenance)
      : _gid(std::move(gid)), _maintenance(std::move(maintenance)) {}
#endif

 public:
  explicit DocumentStateShardHandler(
      TRI_vocbase_t& vocbase, GlobalLogIdentifier gid,
      std::shared_ptr<IMaintenanceActionExecutor> maintenance);
  auto ensureShard(ShardID shard, CollectionID collection,
                   std::shared_ptr<VPackBuilder> properties)
      -> ResultT<bool> override;
  auto modifyShard(ShardID shard, CollectionID collection,
                   std::shared_ptr<VPackBuilder> properties,
                   std::string followersToDrop) -> ResultT<bool> override;
  auto dropShard(ShardID const& shard) -> ResultT<bool> override;
  auto dropAllShards() -> Result override;
  auto isShardAvailable(ShardID const& shardId) -> bool override;
  auto getShardMap() -> ShardMap override;

 private:
  GlobalLogIdentifier _gid;
  std::shared_ptr<IMaintenanceActionExecutor> _maintenance;
  struct {
    ShardMap shards;
    std::shared_mutex mutex;
  } _shardMap;
};

}  // namespace arangodb::replication2::replicated_state::document
