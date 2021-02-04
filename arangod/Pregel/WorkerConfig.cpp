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

#include "Pregel/WorkerConfig.h"
#include "Pregel/Algorithm.h"
#include "Pregel/PregelFeature.h"
#include "Pregel/Utils.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::pregel;

namespace {
std::vector<ShardID> const emptyEdgeCollectionRestrictions;
}

WorkerConfig::WorkerConfig(TRI_vocbase_t* vocbase, VPackSlice params)
    : _vocbase(vocbase) {
  updateConfig(params);
}

void WorkerConfig::updateConfig(VPackSlice params) {
  VPackSlice coordID = params.get(Utils::coordinatorIdKey);
  VPackSlice vertexShardMap = params.get(Utils::vertexShardsKey);
  VPackSlice edgeShardMap = params.get(Utils::edgeShardsKey);
  VPackSlice edgeCollectionRestrictions = params.get(Utils::edgeCollectionRestrictionsKey);
  VPackSlice execNum = params.get(Utils::executionNumberKey);
  VPackSlice collectionPlanIdMap = params.get(Utils::collectionPlanIdMapKey);
  VPackSlice globalShards = params.get(Utils::globalShardListKey);
  VPackSlice async = params.get(Utils::asyncModeKey);

  if (!coordID.isString() || !edgeShardMap.isObject() ||
      !vertexShardMap.isObject() || !execNum.isInteger() ||
      !collectionPlanIdMap.isObject() || !globalShards.isArray()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "Supplied bad parameters to worker");
  }
  _executionNumber = execNum.getUInt();
  _coordinatorId = coordID.copyString();
  _asynchronousMode = async.getBool();
  _useMemoryMaps = params.get(Utils::useMemoryMapsKey).getBool();

  VPackSlice userParams = params.get(Utils::userParametersKey);
  VPackSlice parallel = userParams.get(Utils::parallelismKey);
  
  size_t maxP = PregelFeature::availableParallelism();
  _parallelism = std::max<size_t>(1, std::min<size_t>(maxP / 4, 16));
  if (parallel.isInteger()) {
    _parallelism = std::min<size_t>(std::max<size_t>(1, parallel.getUInt()), maxP);
  }

  // list of all shards, equal on all workers. Used to avoid storing strings of
  // shard names
  // Instead we have an index identifying a shard name in this vector
  PregelShard i = 0;
  for (VPackSlice shard : VPackArrayIterator(globalShards)) {
    ShardID s = shard.copyString();
    _globalShardIDs.push_back(s);
    _pregelShardIDs.try_emplace(s, i++);  // Cache these ids
  }

  // To access information based on a user defined collection name we need the
  for (auto it : VPackObjectIterator(collectionPlanIdMap)) {
    _collectionPlanIdMap.try_emplace(it.key.copyString(), it.value.copyString());
  }

  // Ordered list of shards for each vertex collection on the CURRENT db server
  // Order matters because the for example the third vertex shard, will only
  // every have
  // edges in the third edge shard. This should speed up the startup
  for (auto pair : VPackObjectIterator(vertexShardMap)) {
    CollectionID cname = pair.key.copyString();
    
    std::vector<ShardID> shards;
    for (VPackSlice shardSlice : VPackArrayIterator(pair.value)) {
      ShardID shard = shardSlice.copyString();
      shards.push_back(shard);
      _localVertexShardIDs.push_back(shard);
      _localPregelShardIDs.insert(_pregelShardIDs[shard]);
      _localPShardIDs_hash.insert(_pregelShardIDs[shard]);
      _shardToCollectionName.try_emplace(shard, cname);
    }
    _vertexCollectionShards.try_emplace(std::move(cname), std::move(shards));
  }

  // Ordered list of edge shards for each collection
  for (auto pair : VPackObjectIterator(edgeShardMap)) {
    CollectionID cname = pair.key.copyString();
    
    std::vector<ShardID> shards;
    for (VPackSlice shardSlice : VPackArrayIterator(pair.value)) {
      ShardID shard = shardSlice.copyString();
      shards.push_back(shard);
      _localEdgeShardIDs.push_back(shard);
      _shardToCollectionName.try_emplace(shard, cname);
    }
    _edgeCollectionShards.try_emplace(std::move(cname), std::move(shards));
  }

  if (edgeCollectionRestrictions.isObject()) {
    for (auto pair : VPackObjectIterator(edgeCollectionRestrictions)) {
      CollectionID cname = pair.key.copyString();
    
      std::vector<ShardID> shards;
      for (VPackSlice shardSlice : VPackArrayIterator(pair.value)) {
        shards.push_back(shardSlice.copyString());
      }
      _edgeCollectionRestrictions.try_emplace(std::move(cname), std::move(shards));
    }
  }
}
  
std::vector<ShardID> const& WorkerConfig::edgeCollectionRestrictions(ShardID const& shard) const {
  auto it = _edgeCollectionRestrictions.find(shard);
  if (it != _edgeCollectionRestrictions.end()) {
    return (*it).second;
  }
  return ::emptyEdgeCollectionRestrictions;
}

PregelID WorkerConfig::documentIdToPregel(std::string const& documentID) const {
  size_t pos = documentID.find("/");
  if (pos == std::string::npos) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_FORBIDDEN,
                                   "not a valid document id");
  }
  VPackStringRef docRef(documentID);
  VPackStringRef collPart = docRef.substr(0, pos);
  VPackStringRef keyPart = docRef.substr(pos + 1);

  ShardID responsibleShard;

  auto& ci = _vocbase->server().getFeature<ClusterFeature>().clusterInfo();
  Utils::resolveShard(ci, this, collPart.toString(), StaticStrings::KeyString,
                      keyPart, responsibleShard);

  PregelShard source = this->shardId(responsibleShard);
  return PregelID(source, keyPart.toString());
}
