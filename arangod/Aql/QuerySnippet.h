////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_AQL_QUERY_SNIPPET_H
#define ARANGOD_AQL_QUERY_SNIPPET_H 1

#include "Aql/Query.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ResultT.h"

#include <map>
#include <set>
#include <vector>

namespace arangodb {
namespace aql {

class DistributeConsumerNode;
class ExecutionNode;
class ExecutionPlan;
class GatherNode;
class RemoteNode;
class ScatterNode;
class ShardLocking;

using MapNodeToColNameToShards = std::unordered_map<ExecutionNode*, std::unordered_map<std::string, std::set<ShardID>>>;

class QuerySnippet {
 public:
  using Id = size_t;
 private:
  struct ExpansionInformation {
    ExecutionNode* node;
    bool doExpand;
    bool isSatellite;

    ExpansionInformation() = delete;
    ExpansionInformation(ExecutionNode* n, bool exp, bool sat)
        : node(n), doExpand(exp), isSatellite(sat) {}
  };

 public:
  QuerySnippet(GatherNode const* sinkNode, ExecutionNodeId idOfSinkRemoteNode, Id id)
      : _sinkNode(sinkNode),
        _idOfSinkRemoteNode(idOfSinkRemoteNode),
        _madeResponsibleForShutdown(false),
        _inputSnippet(0),
        _globalScatter(nullptr),
        _id(id) {
    TRI_ASSERT(_sinkNode != nullptr);
    TRI_ASSERT(_idOfSinkRemoteNode != ExecutionNodeId{0});
  }

  void addNode(ExecutionNode* node);

  void serializeIntoBuilder(ServerID const& server,
                            std::unordered_map<ExecutionNodeId, ExecutionNode*> const& nodesById,
                            ShardLocking& shardMapping,
                            std::map<ExecutionNodeId, ExecutionNodeId>& nodeAliases,
                            velocypack::Builder& infoBuilder);

  void useQueryIdAsInput(QueryId inputSnippet) { _inputSnippet = inputSnippet; }

  Id id() const { return _id; }

 private:
  ResultT<MapNodeToColNameToShards> prepareFirstBranch(
      ServerID const& server,
      std::unordered_map<ExecutionNodeId, ExecutionNode*> const& nodesById,
      ShardLocking& shardLocking);

 private:
  GatherNode const* _sinkNode; // node that merges the results for all shards

  ExecutionNodeId const _idOfSinkRemoteNode;

  RemoteNode * _remoteNode{nullptr};

  bool _madeResponsibleForShutdown;

  std::vector<ExecutionNode*> _nodes;

  std::vector<ExpansionInformation> _expansions;

  QueryId _inputSnippet;

  ScatterNode* _globalScatter;

  Id const _id;
};
}  // namespace aql
}  // namespace arangodb

#endif
