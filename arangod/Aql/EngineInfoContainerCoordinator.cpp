////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include "Aql/BlocksWithClients.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/Query.h"
#include "Aql/SharedQueryState.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"

using namespace arangodb;
using namespace arangodb::aql;

// -----------------------------------------------------------------------------
// --SECTION--                                             Coordinator Container
// -----------------------------------------------------------------------------

EngineInfoContainerCoordinator::EngineInfo::EngineInfo(
    EngineId id, ExecutionNodeId idOfRemoteNode)
    : _id(id), _idOfRemoteNode(idOfRemoteNode) {
  TRI_ASSERT(_nodes.empty());
}

EngineInfoContainerCoordinator::EngineInfo::EngineInfo(
    EngineInfo&& other) noexcept
    : _id(other._id),
      _nodes(std::move(other._nodes)),
      _idOfRemoteNode(other._idOfRemoteNode) {}

void EngineInfoContainerCoordinator::EngineInfo::addNode(ExecutionNode* en) {
  _nodes.emplace_back(en);
}

Result EngineInfoContainerCoordinator::EngineInfo::buildEngine(
    Query& query, MapRemoteToSnippet const& dbServerQueryIds, bool isFirst,
    std::unique_ptr<ExecutionEngine>& engine) const {
  TRI_ASSERT(!_nodes.empty());

  if (isFirst) {
    // use shared state of Query
    engine = std::make_unique<ExecutionEngine>(
        _id, query, query.itemBlockManager(), query.sharedState());
  } else {
    // create a separate shared state
    engine = std::make_unique<ExecutionEngine>(
        _id, query, query.itemBlockManager(),
        std::make_shared<SharedQueryState>(query.vocbase().server()));
  }

  auto res = engine->createBlocks(_nodes, dbServerQueryIds);
  if (!res.ok()) {
    engine.reset();
  } else {
    TRI_ASSERT(engine->root() != nullptr);
  }

  return res;
}

EngineId EngineInfoContainerCoordinator::EngineInfo::engineId() const {
  return _id;
}

EngineInfoContainerCoordinator::EngineInfoContainerCoordinator() {
  // We always start with an empty coordinator snippet
  _engines.emplace_back(QueryId{0}, ExecutionNodeId{0});
  _engineStack.emplace(0);
}

EngineInfoContainerCoordinator::~EngineInfoContainerCoordinator() = default;

void EngineInfoContainerCoordinator::addNode(ExecutionNode* node) {
  TRI_ASSERT(node->getType() != ExecutionNode::INDEX &&
             node->getType() != ExecutionNode::ENUMERATE_COLLECTION &&
             node->getType() != ExecutionNode::JOIN);

  TRI_ASSERT(!_engines.empty());
  TRI_ASSERT(!_engineStack.empty());
  size_t idx = _engineStack.top();
  _engines[idx].addNode(node);
}

void EngineInfoContainerCoordinator::openSnippet(
    ExecutionNodeId idOfRemoteNode) {
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
    Query& query, AqlItemBlockManager& mgr,
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
      TRI_ASSERT(info.engineId() == engine->engineId());

      first = false;
      coordSnippets.emplace_back(std::move(engine));
    }
  } catch (basics::Exception const& ex) {
    return {ex.code(), ex.message()};
  } catch (std::exception const& ex) {
    return {TRI_ERROR_INTERNAL, ex.what()};
  } catch (...) {
    return {TRI_ERROR_INTERNAL};
  }

  return {};
}
