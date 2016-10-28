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

#include "WorkerContext.h"
#include "Algorithm.h"
#include "IncomingCache.h"
#include "Utils.h"

using namespace arangodb;
using namespace arangodb::pregel;

template <typename V, typename E, typename M>
WorkerContext<V, E, M>::WorkerContext(Algorithm<V, E, M>* algo,
                                      DatabaseID dbname, VPackSlice params)
    : _algorithm(algo), _database(dbname) {
  VPackSlice coordID = params.get(Utils::coordinatorIdKey);
  VPackSlice vertexCollName = params.get(Utils::vertexCollectionNameKey);
  VPackSlice vertexCollPlanId = params.get(Utils::vertexCollectionPlanIdKey);
  VPackSlice vertexShardIDs = params.get(Utils::vertexShardsListKey);
  VPackSlice edgeShardIDs = params.get(Utils::edgeShardsListKey);
  VPackSlice execNum = params.get(Utils::executionNumberKey);
  if (!coordID.isString() || !vertexCollName.isString() ||
      !vertexCollPlanId.isString() || !vertexShardIDs.isArray() ||
      !edgeShardIDs.isArray() || !execNum.isInteger()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "Supplied bad parameters to worker");
  }
  _executionNumber = execNum.getUInt();
  _coordinatorId = coordID.copyString();
  _vertexCollectionName = vertexCollName.copyString();
  _vertexCollectionPlanId = vertexCollPlanId.copyString();

  LOG(INFO) << "Local Shards:";
  VPackArrayIterator vertices(vertexShardIDs);
  for (VPackSlice shardSlice : vertices) {
    ShardID name = shardSlice.copyString();
    _localVertexShardIDs.push_back(name);
    LOG(INFO) << name;
  }
  VPackArrayIterator edges(edgeShardIDs);
  for (VPackSlice shardSlice : edges) {
    ShardID name = shardSlice.copyString();
    _localEdgeShardIDs.push_back(name);
    LOG(INFO) << name;
  }

  auto format = algo->messageFormat();
  auto combiner = algo->messageCombiner();
  _readCache = std::make_shared<IncomingCache<M>>(format, combiner);
  _writeCache = std::make_shared<IncomingCache<M>>(format, combiner);
}

template <typename V, typename E, typename M>
void WorkerContext<V, E, M>::swapIncomingCaches() {
  auto t = _readCache;
  _readCache = _writeCache;
  _writeCache = t;
}

// template types to create
template class arangodb::pregel::WorkerContext<int64_t, int64_t, int64_t>;
