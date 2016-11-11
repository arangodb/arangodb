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

#ifndef ARANGODB_PREGEL_WORKER_STATE_H
#define ARANGODB_PREGEL_WORKER_STATE_H 1

#include <velocypack/velocypack-aliases.h>
#include "Basics/Common.h"
#include "Cluster/ClusterInfo.h"

namespace arangodb {
class SingleCollectionTransaction;
namespace pregel {
  
template <typename V, typename E, typename M>
class Worker;

////////////////////////////////////////////////////////////////////////////////
/// @brief carry common parameters
////////////////////////////////////////////////////////////////////////////////
class WorkerState {
  template <typename V, typename E, typename M>
  friend class Worker;

 public:
  WorkerState(DatabaseID dbname, VPackSlice params);

  inline uint64_t executionNumber() { return _executionNumber; }

  inline uint64_t globalSuperstep() { return _globalSuperstep; }

  inline std::string const& coordinatorId() const { return _coordinatorId; }

  inline std::string const& database() const { return _database; }

  inline std::vector<ShardID> const& localVertexShardIDs() const {
    return _localVertexShardIDs;
  }

  inline std::vector<ShardID> const& localEdgeShardIDs() const {
    return _localEdgeShardIDs;
  }

  std::map<CollectionID, std::string> const& collectionPlanIdMap() {
    return _collectionPlanIdMap;
  };
  
  //inline uint64_t numWorkerThreads() {
  //  return _numWorkerThreads;
  //}

 private:
  uint64_t _executionNumber;
  uint64_t _globalSuperstep = 0;
  //uint64_t _numWorkerThreads = 1;

  std::string _coordinatorId;
  const std::string _database;
  std::vector<ShardID> _localVertexShardIDs, _localEdgeShardIDs;
  std::map<std::string, std::string> _collectionPlanIdMap;
};
}
}
#endif
