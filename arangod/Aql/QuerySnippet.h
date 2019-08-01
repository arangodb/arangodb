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

#include <map>
#include <vector>

namespace arangodb {
namespace aql {

class ExecutionNode;
class GatherNode;

class QuerySnippet {
 private:
  struct ExpansionInformation {
    ExecutionNode* node;
    bool doExpand;
    std::shared_ptr<std::vector<ShardID>> shards;
    bool isSatellite;

    ExpansionInformation() = delete;
    ExpansionInformation(ExecutionNode* n, bool exp,
                         std::shared_ptr<std::vector<ShardID>> s, bool sat)
        : node(n), doExpand(exp), shards(s), isSatellite(sat) {
      TRI_ASSERT(shards != nullptr);
    }
  };

 public:
  QuerySnippet(GatherNode const* sinkNode, size_t idOfSinkRemoteNode)
      : _sinkNode(sinkNode),
        _idOfSinkRemoteNode(idOfSinkRemoteNode),
        _madeResponsibleForShutdown(false),
        _inputSnippet(0) {
    TRI_ASSERT(_sinkNode != nullptr);
    TRI_ASSERT(_idOfSinkRemoteNode != 0);
  }

  void addNode(ExecutionNode* node);

  void serializeIntoBuilder(ServerID const& server,
                            std::unordered_map<ShardID, ServerID> const& shardMapping,
                            velocypack::Builder& infoBuilder);

  void useQueryIdAsInput(QueryId inputSnippet) { _inputSnippet = inputSnippet; }

 private:
  GatherNode const* _sinkNode;

  size_t const _idOfSinkRemoteNode;

  bool _madeResponsibleForShutdown;

  std::vector<ExecutionNode*> _nodes;

  std::vector<ExpansionInformation> _expansions;

  QueryId _inputSnippet;
};
}  // namespace aql
}  // namespace arangodb

#endif