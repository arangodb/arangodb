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

#ifndef ARANGODB_PREGEL_WORKER_CONFIG_H
#define ARANGODB_PREGEL_WORKER_CONFIG_H 1

#include <set>
#include <map>

#include <velocypack/velocypack-aliases.h>
#include <algorithm>
#include "Basics/Common.h"
#include "Cluster/ClusterInfo.h"
#include "Pregel/Graph.h"

struct TRI_vocbase_t;
namespace arangodb {
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
  WorkerConfig(TRI_vocbase_t* vocbase, VPackSlice params);
  void updateConfig(VPackSlice updated);

  inline uint64_t executionNumber() const { return _executionNumber; }

  inline uint64_t globalSuperstep() const { return _globalSuperstep; }

  inline uint64_t localSuperstep() const { return _localSuperstep; }

  inline bool asynchronousMode() const { return _asynchronousMode; }

  inline bool useMemoryMaps() const { return _useMemoryMaps; }

  inline uint64_t parallelism() const { return _parallelism; }

  inline std::string const& coordinatorId() const { return _coordinatorId; }

  inline TRI_vocbase_t* vocbase() const { return _vocbase; }
  inline std::string const& database() const { return _vocbase->name(); }

  // collection shards on this worker
  inline std::map<CollectionID, std::vector<ShardID>> const& vertexCollectionShards() const {
    return _vertexCollectionShards;
  }

  // collection shards on this worker
  inline std::map<CollectionID, std::vector<ShardID>> const& edgeCollectionShards() const {
    return _edgeCollectionShards;
  }

  inline std::unordered_map<CollectionID, std::string> const& collectionPlanIdMap() const {
    return _collectionPlanIdMap;
  }
  
  std::string const& shardIDToCollectionName(ShardID const& shard) const {
    auto const& it = _shardToCollectionName.find(shard);
    if (it != _shardToCollectionName.end()) {
      return it->second;
    }
    return StaticStrings::Empty;
  }

  // same content on every worker, has to stay equal!!!!
  inline std::vector<ShardID> const& globalShardIDs() const {
    return _globalShardIDs;
  }

  // convenvience access without guaranteed order, same values as in
  // vertexCollectionShards
  inline std::vector<ShardID> const& localVertexShardIDs() const {
    return _localVertexShardIDs;
  }

  // convenvience access without guaranteed order, same values as in
  // edgeCollectionShards
  inline std::vector<ShardID> const& localEdgeShardIDs() const {
    return _localEdgeShardIDs;
  }

  /// Actual set of pregel shard id's located here
  inline std::set<PregelShard> const& localPregelShardIDs() const {
    return _localPregelShardIDs;
  }

  inline PregelShard shardId(ShardID const& responsibleShard) const {
    auto const& it = _pregelShardIDs.find(responsibleShard);
    return it != _pregelShardIDs.end() ? it->second : InvalidPregelShard;
  }

  // index in globalShardIDs
  inline bool isLocalVertexShard(PregelShard shardIndex) const {
    // TODO cache this? prob small
    return _localPShardIDs_hash.find(shardIndex) != _localPShardIDs_hash.end();
  }
  
  std::vector<ShardID> const& edgeCollectionRestrictions(ShardID const& shard) const;

  // convert an arangodb document id to a pregel id
  PregelID documentIdToPregel(std::string const& documentID) const;

 private:
  uint64_t _executionNumber = 0;
  uint64_t _globalSuperstep = 0;
  uint64_t _localSuperstep = 0;

  /// Let async
  bool _asynchronousMode = false;
  bool _useMemoryMaps = false; /// always use mmaps

  size_t _parallelism = 1;

  std::string _coordinatorId;
  TRI_vocbase_t* _vocbase;

  std::vector<ShardID> _globalShardIDs;
  std::vector<ShardID> _localVertexShardIDs, _localEdgeShardIDs;

  std::unordered_map<std::string, std::string> _collectionPlanIdMap;
  std::map<ShardID, std::string> _shardToCollectionName;

  // Map from edge collection to their shards, only iterated over keep sorted
  std::map<CollectionID, std::vector<ShardID>> _vertexCollectionShards, _edgeCollectionShards;
  
  std::unordered_map<CollectionID, std::vector<ShardID>> _edgeCollectionRestrictions;

  /// cache these ids as much as possible, since we access them often
  std::unordered_map<std::string, PregelShard> _pregelShardIDs;
  std::set<PregelShard> _localPregelShardIDs;
  std::unordered_set<PregelShard> _localPShardIDs_hash;
};
}  // namespace pregel
}  // namespace arangodb
#endif
