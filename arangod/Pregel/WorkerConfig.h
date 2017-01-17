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

#ifndef ARANGODB_PREGEL_WORKER_CONFIG_H
#define ARANGODB_PREGEL_WORKER_CONFIG_H 1

#include <velocypack/velocypack-aliases.h>
#include <algorithm>
#include "Basics/Common.h"
#include "Cluster/ClusterInfo.h"
#include "Pregel/Graph.h"

namespace arangodb {
class SingleCollectionTransaction;
namespace pregel {

template <typename V, typename E, typename M>
class Worker;

////////////////////////////////////////////////////////////////////////////////
/// @brief carry common parameters
////////////////////////////////////////////////////////////////////////////////
class WorkerConfig {
  template <typename V, typename E, typename M>
  friend class Worker;

 public:
  WorkerConfig(DatabaseID dbname, VPackSlice params);

  inline uint64_t executionNumber() const { return _executionNumber; }

  inline uint64_t globalSuperstep() const { return _globalSuperstep; }

  inline uint64_t localSuperstep() const { return _localSuperstep; }

  inline bool asynchronousMode() const { return _asynchronousMode; }

  inline bool lazyLoading() const { return _lazyLoading; }

  inline std::string const& coordinatorId() const { return _coordinatorId; }

  inline std::string const& database() const { return _database; }

  // collection shards on this worker
  inline std::map<CollectionID, std::vector<ShardID>> const&
  vertexCollectionShards() const {
    return _vertexCollectionShards;
  }

  // collection shards on this worker
  inline std::map<CollectionID, std::vector<ShardID>> const&
  edgeCollectionShards() const {
    return _edgeCollectionShards;
  }

  inline std::map<CollectionID, std::string> const& collectionPlanIdMap()
      const {
    return _collectionPlanIdMap;
  };

  // same content on every worker, has to stay equal!!!!
  inline std::vector<ShardID> const& globalShardIDs() const {
    return _globalShardIDs;
  };

  // convenvience access without guaranteed order, same values as in
  // vertexCollectionShards
  inline std::vector<ShardID> const& localVertexShardIDs() const {
    return _localVertexShardIDs;
  };
  // convenvience access without guaranteed order, same values as in
  // edgeCollectionShards
  inline std::vector<ShardID> const& localEdgeShardIDs() const {
    return _localEdgeShardIDs;
  };
  inline prgl_shard_t shardId(ShardID const& responsibleShard) const {
    auto it = std::find(_globalShardIDs.begin(), _globalShardIDs.end(),
                        responsibleShard);
    return it != _globalShardIDs.end() ? it - _globalShardIDs.begin()
                                       : (prgl_shard_t)-1;
  }
  // index in globalShardIDs
  inline bool isLocalVertexShard(prgl_shard_t shardIndex) const {
    // TODO cache this? prob small
    ShardID const& shard = _globalShardIDs[shardIndex];
    return std::find(_localVertexShardIDs.begin(), _localVertexShardIDs.end(),
                     shard) != _localVertexShardIDs.end();
  }

  PregelID documentIdToPregel(std::string const& documentID) const;

 private:
  uint64_t _executionNumber = 0;
  uint64_t _globalSuperstep = 0;
  uint64_t _localSuperstep = 0;

  bool _asynchronousMode = false;
  /// load vertices on a lazy basis
  bool _lazyLoading = false;

  std::string _coordinatorId;
  std::string _database;

  std::vector<ShardID> _globalShardIDs;
  std::vector<ShardID> _localVertexShardIDs, _localEdgeShardIDs;

  std::map<CollectionID, std::vector<ShardID>> _vertexCollectionShards,
      _edgeCollectionShards;
  std::map<std::string, std::string> _collectionPlanIdMap;
};
}
}
#endif
