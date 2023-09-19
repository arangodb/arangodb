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

#include "Cluster/ClusterTypes.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "VocBase/Methods/Indexes.h"

struct TRI_vocbase_t;
namespace arangodb {
class MaintenanceFeature;
}

namespace arangodb::replication2::replicated_state::document {

struct IMaintenanceActionExecutor {
  virtual ~IMaintenanceActionExecutor() = default;
  virtual auto executeCreateCollectionAction(
      ShardID shard, CollectionID collection,
      std::shared_ptr<VPackBuilder> properties) -> Result = 0;
  virtual auto executeDropCollectionAction(ShardID shard,
                                           CollectionID collection)
      -> Result = 0;
  virtual auto executeModifyCollectionAction(
      ShardID shard, CollectionID collection,
      std::shared_ptr<VPackBuilder> properties, std::string followersToDrop)
      -> Result = 0;
  virtual auto executeCreateIndex(
      ShardID shard, std::shared_ptr<VPackBuilder> const& properties,
      std::shared_ptr<methods::Indexes::ProgressTracker> progress)
      -> Result = 0;
  virtual auto executeDropIndex(ShardID shard, velocypack::SharedSlice index)
      -> Result = 0;
  virtual void addDirty() = 0;
};

class MaintenanceActionExecutor : public IMaintenanceActionExecutor {
 public:
  MaintenanceActionExecutor(GlobalLogIdentifier _gid, ServerID server,
                            MaintenanceFeature& maintenanceFeature,
                            TRI_vocbase_t& vocbase);
  auto executeCreateCollectionAction(ShardID shard, CollectionID collection,
                                     std::shared_ptr<VPackBuilder> properties)
      -> Result override;
  auto executeDropCollectionAction(ShardID shard, CollectionID collection)
      -> Result override;
  auto executeModifyCollectionAction(ShardID shard, CollectionID collection,
                                     std::shared_ptr<VPackBuilder> properties,
                                     std::string followersToDrop)
      -> Result override;
  auto executeCreateIndex(
      ShardID shard, std::shared_ptr<VPackBuilder> const& properties,
      std::shared_ptr<methods::Indexes::ProgressTracker> progress)
      -> Result override;
  auto executeDropIndex(ShardID shard, velocypack::SharedSlice index)
      -> Result override;
  void addDirty() override;

 private:
  GlobalLogIdentifier _gid;
  MaintenanceFeature& _maintenanceFeature;
  ServerID _server;

  // The vocbase reference remains valid for the lifetime of the executor.
  // Replicated logs are stopped before the vocbase is marked as dropped.
  TRI_vocbase_t& _vocbase;
};
}  // namespace arangodb::replication2::replicated_state::document
