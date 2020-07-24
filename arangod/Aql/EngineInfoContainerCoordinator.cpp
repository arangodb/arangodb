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

#include "EngineInfoContainerCoordinator.h"

#include "Aql/AqlItemBlockSerializationFormat.h"
#include "Aql/BlocksWithClients.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNode.h"
#include "Aql/Query.h"
#include "Aql/QueryRegistry.h"
#include "Basics/ScopeGuard.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "VocBase/ticks.h"

using namespace arangodb;
using namespace arangodb::aql;

// -----------------------------------------------------------------------------
// --SECTION--                                             Coordinator Container
// -----------------------------------------------------------------------------

EngineInfoContainerCoordinator::EngineInfo::EngineInfo(EngineId id, ExecutionNodeId idOfRemoteNode)
    : _id(id), _idOfRemoteNode(idOfRemoteNode) {
  TRI_ASSERT(_nodes.empty());
}

EngineInfoContainerCoordinator::EngineInfo::EngineInfo(EngineInfo&& other) noexcept
    : _id(other._id),
      _nodes(std::move(other._nodes)),
      _idOfRemoteNode(other._idOfRemoteNode) {}

void EngineInfoContainerCoordinator::EngineInfo::addNode(ExecutionNode* en) {
  _nodes.emplace_back(en);
}

Result EngineInfoContainerCoordinator::EngineInfo::buildEngine(
    Query& query, MapRemoteToSnippet const& dbServerQueryIds,
    bool isfirst, std::unique_ptr<ExecutionEngine>& engine) const {
  TRI_ASSERT(!_nodes.empty());
  
  std::shared_ptr<SharedQueryState> sqs;
  if (isfirst) {
    sqs = query.sharedState();
  }
  
  engine = std::make_unique<ExecutionEngine>(query, query.itemBlockManager(),
                                             SerializationFormat::SHADOWROWS, sqs);

  auto res = engine->createBlocks(_nodes, dbServerQueryIds);
  if (!res.ok()) {
    engine.reset();
    return res;
  }

  TRI_ASSERT(engine->root() != nullptr);

  return {TRI_ERROR_NO_ERROR};
}

EngineId EngineInfoContainerCoordinator::EngineInfo::engineId() const { return _id; }

EngineInfoContainerCoordinator::EngineInfoContainerCoordinator() {
  // We always start with an empty coordinator snippet
  _engines.emplace_back(QueryId{0}, ExecutionNodeId{0});
  _engineStack.emplace(0);
}

EngineInfoContainerCoordinator::~EngineInfoContainerCoordinator() = default;

void EngineInfoContainerCoordinator::addNode(ExecutionNode* node) {
  TRI_ASSERT(node->getType() != ExecutionNode::INDEX &&
             node->getType() != ExecutionNode::ENUMERATE_COLLECTION);

  TRI_ASSERT(!_engines.empty());
  TRI_ASSERT(!_engineStack.empty());
  size_t idx = _engineStack.top();
  _engines[idx].addNode(node);
}

void EngineInfoContainerCoordinator::openSnippet(ExecutionNodeId idOfRemoteNode) {
  _engineStack.emplace(_engines.size());  // Insert next id
  QueryId id = TRI_NewTickServer();
  _engines.emplace_back(id, idOfRemoteNode);
}

QueryId EngineInfoContainerCoordinator::closeSnippet() {
  TRI_ASSERT(!_engines.empty());
  TRI_ASSERT(!_engineStack.empty());

  size_t idx = _engineStack.top();
  QueryId id = _engines[idx].engineId();
  _engineStack.pop();
  return id;
}

Result EngineInfoContainerCoordinator::buildEngines(
    Query& query,
    AqlItemBlockManager& mgr,
    MapRemoteToSnippet const& dbServerQueryIds,
    aql::SnippetList& coordSnippets) const {
  TRI_ASSERT(_engineStack.size() == 1);
  TRI_ASSERT(_engineStack.top() == 0);

  try {
    bool first = true;
    for (EngineInfo const& info : _engines) {
      std::unique_ptr<ExecutionEngine> engine;
      auto res = info.buildEngine(query, dbServerQueryIds, first, engine);
      if (res.fail()) {
        return res;
      }
      TRI_ASSERT(!first || info.engineId() == 0);
      TRI_ASSERT(!first || query.sharedState() == engine->sharedState());
      
      first = false;
      coordSnippets.emplace_back(std::make_pair(info.engineId(), std::move(engine)));
    }
  } catch (basics::Exception const& ex) {
    return Result(ex.code(), ex.message());
  } catch (std::exception const& ex) {
    return Result(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    return Result(TRI_ERROR_INTERNAL);
  }

  // This deactivates the defered cleanup.
  // From here on we rely on the AQL shutdown mechanism.
//  guard.cancel();
  return Result();
}
