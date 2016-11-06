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

#include "Algorithm.h"

namespace arangodb {
class SingleCollectionTransaction;
namespace pregel {
typedef unsigned int prglSeq_t;

template <typename M>
class IncomingCache;
template <typename V, typename E, typename M>
class Worker;

////////////////////////////////////////////////////////////////////////////////
/// @brief carry common parameters
////////////////////////////////////////////////////////////////////////////////
template <typename V, typename E, typename M>
class WorkerState {
  friend class Worker<V, E, M>;

 public:
  WorkerState(Algorithm<V, E, M>* algo, DatabaseID dbname, VPackSlice params);

  inline prglSeq_t executionNumber() { return _executionNumber; }

  inline prglSeq_t globalSuperstep() { return _globalSuperstep; }

  inline std::string const& coordinatorId() const { return _coordinatorId; }

  inline std::string const& database() const { return _database; }

  inline std::vector<ShardID> const& localVertexShardIDs() const {
    return _localVertexShardIDs;
  }

  inline std::vector<ShardID> const& localEdgeShardIDs() const {
    return _localEdgeShardIDs;
  }

  // inline ShardID const& edgeShardId() const {
  //    return _edgeShardID;
  //}

  inline std::shared_ptr<IncomingCache<M>> readableIncomingCache() {
    return _readCache;
  }

  inline std::shared_ptr<IncomingCache<M>> writeableIncomingCache() {
    return _writeCache;
  }

  std::shared_ptr<Algorithm<V, E, M>> algorithm() { return _algorithm; }
  
  std::map<CollectionID, std::string> const& collectionPlanIdMap() {return _collectionPlanIdMap;};

 private:
  /// @brief guard to make sure the database is not dropped while used by us
  prglSeq_t _executionNumber;
  const std::shared_ptr<Algorithm<V, E, M>> _algorithm;

  prglSeq_t _globalSuperstep = 0;
  prglSeq_t _expectedGSS = 0;
  std::string _coordinatorId;
  const std::string _database;
  std::vector<ShardID> _localVertexShardIDs, _localEdgeShardIDs;
  std::map<std::string, std::string> _collectionPlanIdMap;

  std::shared_ptr<IncomingCache<M>> _readCache, _writeCache;
  void swapIncomingCaches();  // only call when message receiving is locked
};
}
}
#endif
