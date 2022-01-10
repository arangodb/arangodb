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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "TraverserEngineShardLists.h"

#include "Aql/GraphNode.h"
#include "Aql/QueryContext.h"
#include "Graph/BaseOptions.h"

using namespace arangodb;
using namespace arangodb::aql;

TraverserEngineShardLists::TraverserEngineShardLists(
    GraphNode const* node, ServerID const& server,
    std::unordered_map<ShardID, ServerID> const& shardMapping,
    QueryContext& query)
    : _node(node), _hasShard(false) {
  auto const& edges = _node->edgeColls();
  TRI_ASSERT(!edges.empty());
  auto const& restrictToShards = query.queryOptions().restrictToShards;
#ifdef USE_ENTERPRISE
  transaction::Methods trx{query.newTrxContext()};
#endif
  // Extract the local shards for edge collections.
  for (auto const& col : edges) {
    TRI_ASSERT(col != nullptr);
#ifdef USE_ENTERPRISE
    if (trx.isInaccessibleCollection(col->id())) {
      _inaccessible.insert(col->name());
      _inaccessible.insert(std::to_string(col->id().id()));
    }
#endif
    _edgeCollections.emplace_back(
        getAllLocalShards(shardMapping, server, col->shardIds(restrictToShards),
                          col->isSatellite() && node->isSmart()));
  }
  // Extract vertices
  auto const& vertices = _node->vertexColls();
  // Guaranteed by addGraphNode, this will inject vertex collections
  // in anonymous graph case
  // It might in fact be empty, if we only have edge collections in a graph.
  // Or if we guarantee to never read vertex data.
  for (auto const& col : vertices) {
    TRI_ASSERT(col != nullptr);
#ifdef USE_ENTERPRISE
    if (trx.isInaccessibleCollection(col->id())) {
      _inaccessible.insert(col->name());
      _inaccessible.insert(std::to_string(col->id().id()));
    }
#endif
    auto shards =
        getAllLocalShards(shardMapping, server, col->shardIds(restrictToShards),
                          col->isSatellite() && node->isSmart());
    _vertexCollections.try_emplace(col->name(), std::move(shards));
  }
}

std::vector<ShardID> TraverserEngineShardLists::getAllLocalShards(
    std::unordered_map<ShardID, ServerID> const& shardMapping,
    ServerID const& server, std::shared_ptr<std::vector<std::string>> shardIds,
    bool allowReadFromFollower) {
  std::vector<ShardID> localShards;
  for (auto const& shard : *shardIds) {
    auto const& it = shardMapping.find(shard);
    if (it == shardMapping.end()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL,
          "no entry for shard '" + shard + "' in shard mapping table (" +
              std::to_string(shardMapping.size()) + " entries)");
    }
    if (it->second == server) {
      localShards.emplace_back(shard);
      // Guaranteed that the traversal will be executed on this server.
      _hasShard = true;
    } else if (allowReadFromFollower) {
      // The satellite does not force run of a traversal here.
      localShards.emplace_back(shard);
    }
  }
  return localShards;
}

void TraverserEngineShardLists::serializeIntoBuilder(
    VPackBuilder& infoBuilder) const {
  TRI_ASSERT(_hasShard);
  TRI_ASSERT(infoBuilder.isOpenArray());
  infoBuilder.openObject();
  {
    // Options
    infoBuilder.add(VPackValue("options"));
    graph::BaseOptions* opts = _node->options();
    opts->buildEngineInfo(infoBuilder);
  }
  {
    // Variables
    std::vector<aql::Variable const*> vars;
    _node->getConditionVariables(vars);
    if (!vars.empty()) {
      infoBuilder.add(VPackValue("variables"));
      infoBuilder.openArray();
      for (auto v : vars) {
        v->toVelocyPack(infoBuilder);
      }
      infoBuilder.close();
    }
  }

  infoBuilder.add(VPackValue("shards"));
  infoBuilder.openObject();
  infoBuilder.add(VPackValue("vertices"));
  infoBuilder.openObject();
  for (auto const& col : _vertexCollections) {
    infoBuilder.add(VPackValue(col.first));
    infoBuilder.openArray();
    for (auto const& v : col.second) {
      infoBuilder.add(VPackValue(v));
    }
    infoBuilder.close();  // this collection
  }
  infoBuilder.close();  // vertices

  infoBuilder.add(VPackValue("edges"));
  infoBuilder.openArray();
  for (auto const& edgeShards : _edgeCollections) {
    infoBuilder.openArray();
    for (auto const& e : edgeShards) {
      infoBuilder.add(VPackValue(e));
    }
    infoBuilder.close();
  }
  infoBuilder.close();  // edges
  infoBuilder.close();  // shards

  _node->enhanceEngineInfo(infoBuilder);

  infoBuilder.close();  // base
  TRI_ASSERT(infoBuilder.isOpenArray());
}
