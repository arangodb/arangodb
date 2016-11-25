////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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

#include "Cluster/ClusterInfo.h"
#include "Cluster/AgencyComm.h"
#include "Cluster/AgencyCallbackRegistry.h"
#include <velocypack/velocypack-aliases.h>
#include <velocypack/vpack.h>

namespace arangodb {
namespace pregel {

class Conductor;
template<typename V, typename E>
class GraphStore;
  
class RecoveryManager {
  
  AgencyComm _agency;
  AgencyCallbackRegistry *_agencyCallbackRegistry;//weak
  Conductor *_conductor;

  std::map<ServerID, std::string> _statusMap;
  std::vector<std::shared_ptr<AgencyCallback>> _agencyCallbacks;
  
 public:
  RecoveryManager(Conductor *c);
  ~RecoveryManager();

  void monitorDBServers(std::vector<ServerID> const& dbServers);
  bool allServersAvailable(std::vector<ServerID> const& dbServers);
};
  
class RecoveryWorker {
  friend class RestPregelHandler;
  
  std::map<ShardID, ServerID> _secondaries;
  ServerID const* secondaryForShard(ShardID const& shard) {return nullptr;}
  
  //receivedBackupData(VPackSlice slice);
  
public:
  template<typename V, typename E>
  void replicateGraphData(GraphStore<V,E> *graphStore) {}
  
  void reloadPlanData() {_secondaries.clear();}
};
}
}
#endif
