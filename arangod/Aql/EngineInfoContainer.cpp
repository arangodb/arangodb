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

#include "EngineInfoContainer.h"

#include "Aql/ExecutionNode.h"
#include "Logger/Logger.h"
#include "VocBase/ticks.h"

using namespace arangodb;
using namespace arangodb::aql;

EngineInfoContainer::EngineInfo::EngineInfo(size_t id,
                                            std::vector<ExecutionNode*>&& nodes,
                                            size_t idOfRemoteNode)
    : _id(id), _nodes(nodes), _idOfRemoteNode(idOfRemoteNode), _otherId(0) {}

EngineInfoContainer::EngineInfo::~EngineInfo() {
  // This container is not responsible for nodes, they are managed by the AST
  // somewhere else
}

void EngineInfoContainer::EngineInfo::connectQueryId(QueryId id) {
  _otherId = id;
}

EngineInfoContainer::EngineInfoContainer() {}

EngineInfoContainer::~EngineInfoContainer() {}

QueryId EngineInfoContainer::addQuerySnippet(std::vector<ExecutionNode*> nodes,
                                             size_t idOfRemoteNode) {
  // TODO: Check if the following is true:
  // idOfRemote === 0 => id === 0
  QueryId id = TRI_NewTickServer();
  _engines.emplace_back(id, std::move(nodes), idOfRemoteNode);
  // TODO analyse used collections

  return id;
}

void EngineInfoContainer::connectLastSnippet(QueryId id) {
  if (_engines.empty()) {
    // If we do not have engines we cannot append the snippet.
    // This is the case for the initial coordinator snippet.
    return;
  }
  _engines.back().connectQueryId(id);
}
