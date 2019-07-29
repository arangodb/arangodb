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

#include "ClusterInfo.h"

#include <map>
#include <vector>

namespace arangodb {
namespace aql {
class ExecutionNode;

class QuerySnippet {
 private:
  struct ExpansionInformation {
    ExecutionNode* node;
    bool doExpand;
    std::vector<ShardID> shards;
  };

 public:
  QuerySnippet() {}

  void addNode(ExecutionNode* node);

  void serializeIntoBuilder(ServerID const& server, velocypack::Builder& infoBuilder) const;

 private:
  bool _needToInjectGather;

  std::vector<ExecutionNode*> _nodes;

  std::map<ServerID, std::vector<ExpansionInformation>> _expansions;
};
}  // namespace aql
}  // namespace arangodb

#endif