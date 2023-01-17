////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#include "Pregel/Conductor/Messages.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Pregel/Worker/WorkerConfig.h"
#include "Pregel/Algorithm.h"
#include "Pregel/PregelFeature.h"
#include "Pregel/Utils.h"
#include "VocBase/vocbase.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::pregel;

namespace {
std::vector<ShardID> const emptyEdgeCollectionRestrictions;
}

WorkerConfig::WorkerConfig(TRI_vocbase_t* vocbase) : _vocbase(vocbase) {}

std::string const& WorkerConfig::database() const { return _vocbase->name(); }

void WorkerConfig::updateConfig(PregelFeature& feature,
                                CreateWorker const& params) {
  _executionNumber = params.executionNumber;
  _coordinatorId = params.coordinatorId;
  _useMemoryMaps = params.useMemoryMaps;
  VPackSlice userParams = params.userParameters.slice();
  _globalShardIDs = params.allShards;

  // parallelism
  _parallelism = WorkerConfig::parallelism(feature, userParams);

  // list of all shards, equal on all workers. Used to avoid storing strings of
  // shard names
  // Instead we have an index identifying a shard name in this vector
  for (uint16_t i = 0; auto const& shard : _globalShardIDs) {
    _pregelShardIDs.try_emplace(shard, PregelShard(i++));  // Cache these ids
  }

  // To access information based on a user defined collection name we need the
  _collectionPlanIdMap = params.collectionPlanIds;

  // Ordered list of shards for each vertex collection on the CURRENT db server
  // Order matters because the for example the third vertex shard, will only
  // every have
  // edges in the third edge shard. This should speed up the startup
  _vertexCollectionShards = params.vertexShards;
  for (auto const& [collection, shards] : _vertexCollectionShards) {
    for (auto const& shard : shards) {
      _localVertexShardIDs.push_back(shard);
      _localPregelShardIDs.insert(_pregelShardIDs[shard]);
      _localPShardIDs_hash.insert(_pregelShardIDs[shard]);
      _shardToCollectionName.try_emplace(shard, collection);
    }
  }

  // Ordered list of edge shards for each collection
  _edgeCollectionShards = params.edgeShards;
  for (auto const& [collection, shards] : _edgeCollectionShards) {
    for (auto const& shard : shards) {
      _localEdgeShardIDs.push_back(shard);
      _shardToCollectionName.try_emplace(shard, collection);
    }
  }

  _edgeCollectionRestrictions = params.edgeCollectionRestrictions;
}

size_t WorkerConfig::parallelism(PregelFeature& feature, VPackSlice params) {
  // start off with default parallelism
  size_t parallelism = feature.defaultParallelism();
  if (params.isObject()) {
    // then update parallelism value from user config
    if (VPackSlice parallel = params.get(Utils::parallelismKey);
        parallel.isInteger()) {
      // limit parallelism to configured bounds
      parallelism =
          std::clamp(parallel.getNumber<size_t>(), feature.minParallelism(),
                     feature.maxParallelism());
    }
  }
  return parallelism;
}

std::vector<ShardID> const& WorkerConfig::edgeCollectionRestrictions(
    ShardID const& shard) const {
  auto it = _edgeCollectionRestrictions.find(shard);
  if (it != _edgeCollectionRestrictions.end()) {
    return (*it).second;
  }
  return ::emptyEdgeCollectionRestrictions;
}

VertexID WorkerConfig::documentIdToPregel(std::string const& documentID) const {
  size_t pos = documentID.find("/");
  if (pos == std::string::npos) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_FORBIDDEN,
                                   "not a valid document id");
  }
  std::string_view docRef(documentID);
  std::string_view collPart = docRef.substr(0, pos);
  std::string_view keyPart = docRef.substr(pos + 1);

  ShardID responsibleShard;

  auto& ci = _vocbase->server().getFeature<ClusterFeature>().clusterInfo();
  Utils::resolveShard(ci, this, std::string(collPart), StaticStrings::KeyString,
                      keyPart, responsibleShard);

  PregelShard source = this->shardId(responsibleShard);
  return VertexID(source, std::string(keyPart));
}
