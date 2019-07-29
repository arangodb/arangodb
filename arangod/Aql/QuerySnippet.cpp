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

#include "QuerySnippet.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

void QuerySnippet::addNode(ExecutionNode* node) {
  TRI_ASSERT(node != nullptr);
  _nodes.push_back(node);
}

void QuerySnippet::serializeIntoBuilder(ServerID const& server, VPackBuilder& infoBuilder) const {
  TRI_ASSERT(!_nodes.empty());
  TRI_ASSERT(!_expansions.empty());
  auto it = _expansions.find(server);
  if (it == _expansions.end()) {
    // We do not have a snippet for this server sorry.
    return;
  }
  // TODO toVPack all nodes for this specific server
  // We clone every Node* and maintain a list of ReportingGroups for profiler
}
