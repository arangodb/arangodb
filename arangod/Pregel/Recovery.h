////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_PREGEL_RECOVERY_H
#define ARANGODB_PREGEL_RECOVERY_H 1

#include <map>
#include <set>

#include "Basics/Mutex.h"
#include "Cluster/ClusterInfo.h"

namespace arangodb {
namespace pregel {

template <typename V, typename E>
class GraphStore;
class Conductor;

class RecoveryManager {
  ClusterInfo& _ci;
  Mutex _lock;
  AgencyComm _agency;
  // AgencyCallbackRegistry* _agencyCallbackRegistry;  // weak

  std::map<ShardID, std::set<Conductor*>> _listeners;
  std::map<ShardID, ServerID> _primaryServers;
  std::map<ShardID, std::shared_ptr<AgencyCallback>> _agencyCallbacks;

  // void _monitorShard(DatabaseID const& database,
  //                   CollectionID const& cid,
  //                   ShardID const& shard);
  void _renewPrimaryServer(ShardID const& shard);

 public:
  explicit RecoveryManager(ClusterInfo&);
  ~RecoveryManager();

  void monitorCollections(DatabaseID const& database,
                          std::vector<CollectionID> const& collections, Conductor* listener);
  void stopMonitoring(Conductor*);
  ErrorCode filterGoodServers(std::vector<ServerID> const& servers,
                        std::vector<ServerID>& goodServers);
  void updatedFailedServers(std::vector<ServerID> const& failedServers);
  // bool allServersAvailable(std::vector<ServerID> const& dbServers);
};

/*
template
class CheckpointingManager {
  friend class RestPregelHandler;

  std::map<ShardID, ServerID> _secondaries;
  ServerID const* secondaryForShard(ShardID const& shard) { return nullptr; }

  // receivedBackupData(VPackSlice slice);

 public:
  template <typename V, typename E>
  void replicateGraphData(uint64_t exn, uint64_t gss,
 GraphStore<V, E> const* graphStore);

  void restoreGraphData(uint64_t exn, uint64_t gss,
    GraphStore<V, E> const* graphStore);

  void reloadPlanData() { _secondaries.clear(); }
};*/
}  // namespace pregel
}  // namespace arangodb
#endif
