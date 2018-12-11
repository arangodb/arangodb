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
#include "Aql/AqlItemBlock.h"
#include "Aql/AqlResult.h"
#include "Aql/ClusterBlocks.h"
#include "Aql/ClusterNodes.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNode.h"
#include "Aql/Query.h"
#include "Aql/QueryRegistry.h"
#include "VocBase/ticks.h"

using namespace arangodb;
using namespace arangodb::aql;

// -----------------------------------------------------------------------------
// --SECTION--                                             Coordinator Container
// -----------------------------------------------------------------------------

EngineInfoContainerCoordinator::EngineInfo::EngineInfo(QueryId id,
                                                       size_t idOfRemoteNode)
    : _id(id), _idOfRemoteNode(idOfRemoteNode) {
  TRI_ASSERT(_nodes.empty());
}

EngineInfoContainerCoordinator::EngineInfo::~EngineInfo() {
  // This container is not responsible for nodes, they are managed by the AST
  // somewhere else
}

EngineInfoContainerCoordinator::EngineInfo::EngineInfo(EngineInfo const&& other)
    : _id(other._id),
      _nodes(std::move(other._nodes)),
      _idOfRemoteNode(other._idOfRemoteNode) {}

void EngineInfoContainerCoordinator::EngineInfo::addNode(ExecutionNode* en) {
  _nodes.emplace_back(en);
}

Result EngineInfoContainerCoordinator::EngineInfo::buildEngine(
    Query* query, QueryRegistry* queryRegistry, std::string const& dbname,
    std::unordered_set<std::string> const& restrictToShards,
    MapRemoteToSnippet const& dbServerQueryIds,
    std::vector<uint64_t>& coordinatorQueryIds,
    std::unordered_set<ShardID> const& lockedShards) const {
  TRI_ASSERT(!_nodes.empty());
  {
    auto uniqEngine = std::make_unique<ExecutionEngine>(query);
    query->setEngine(uniqEngine.release());
  }

  auto engine = query->engine();

  query->trx()->setLockedShards(lockedShards);

  auto res = engine->createBlocks(_nodes, restrictToShards, dbServerQueryIds);
  if (!res.ok()) {
    return res;
  }

  TRI_ASSERT(engine->root() != nullptr);

  LOG_TOPIC(DEBUG, arangodb::Logger::AQL) << "Storing Coordinator engine: "
                                          << _id;

  // For _id == 0 this thread will always maintain the handle to
  // the engine and will clean up. We do not keep track of it seperately
  if (_id != 0) {
    double ttl = queryRegistry->defaultTTL();
    TRI_ASSERT(ttl > 0);
    try {
      queryRegistry->insert(_id, query, ttl, true, false);
    } catch (basics::Exception const& e) {
      return {e.code(), e.message()};
    } catch (std::exception const& e) {
      return {TRI_ERROR_INTERNAL, e.what()};
    } catch (...) {
      return {TRI_ERROR_INTERNAL};
    }

    coordinatorQueryIds.emplace_back(_id);
  }

  return {TRI_ERROR_NO_ERROR};
}

EngineInfoContainerCoordinator::EngineInfoContainerCoordinator() {
  // We always start with an empty coordinator snippet
  _engines.emplace_back(0, 0);
  _engineStack.emplace(0);
}

EngineInfoContainerCoordinator::~EngineInfoContainerCoordinator() {}

void EngineInfoContainerCoordinator::addNode(ExecutionNode* node) {
  TRI_ASSERT(
    node->getType() != ExecutionNode::INDEX
      && node->getType() != ExecutionNode::ENUMERATE_COLLECTION
  );

  TRI_ASSERT(!_engines.empty());
  TRI_ASSERT(!_engineStack.empty());
  size_t idx = _engineStack.top();
  _engines[idx].addNode(node);
}

void EngineInfoContainerCoordinator::openSnippet(size_t idOfRemoteNode) {
  _engineStack.emplace(_engines.size());  // Insert next id
  QueryId id = TRI_NewTickServer();
  _engines.emplace_back(id, idOfRemoteNode);
}

QueryId EngineInfoContainerCoordinator::closeSnippet() {
  TRI_ASSERT(!_engines.empty());
  TRI_ASSERT(!_engineStack.empty());

  size_t idx = _engineStack.top();
  QueryId id = _engines[idx].queryId();
  _engineStack.pop();
  return id;
}

ExecutionEngineResult EngineInfoContainerCoordinator::buildEngines(
    Query* query, QueryRegistry* registry, std::string const& dbname,
    std::unordered_set<std::string> const& restrictToShards,
    MapRemoteToSnippet const& dbServerQueryIds,
    std::unordered_set<ShardID> const& lockedShards) const {
  TRI_ASSERT(_engineStack.size() == 1);
  TRI_ASSERT(_engineStack.top() == 0);

  std::vector<uint64_t> coordinatorQueryIds{};
  // destroy all query snippets in case of error
  auto guard = scopeGuard([&dbname, &registry, &coordinatorQueryIds]() {
    for (auto const& it : coordinatorQueryIds) {
      registry->destroy(dbname, it, TRI_ERROR_INTERNAL);
    }
  });

  Query* localQuery = query;
  try {
    bool first = true;
    for (auto const& info : _engines) {
      if (!first) {
        // need a new query instance on the coordinator
        localQuery = query->clone(PART_DEPENDENT, false);
        if (localQuery == nullptr) {
          // clone() cannot return nullptr, but some mocks seem to do it
          return ExecutionEngineResult(TRI_ERROR_INTERNAL, "cannot clone query");
        }
        TRI_ASSERT(localQuery != nullptr);
      }
      try {
        auto res = info.buildEngine(localQuery, registry, dbname,
                                    restrictToShards, dbServerQueryIds,
                                    coordinatorQueryIds, lockedShards);
        if (!res.ok()) {
          if (!first) {
            // We need to clean up this query.
            // It is not in the registry.
            delete localQuery;
          }
          return ExecutionEngineResult(res.errorNumber(), res.errorMessage());
        }
      } catch (...) {
        // We do not catch any other error here.
        // All errors we throw are handled by the result
        // above
        if (!first) {
          // We need to clean up this query.
          // It is not in the registry.
          delete localQuery;
        }
        return ExecutionEngineResult(TRI_ERROR_INTERNAL);
      }
      first = false;
    }
  } catch (basics::Exception const& ex) {
    return ExecutionEngineResult(ex.code(), ex.message());
  } catch (std::exception const& ex) {
    return ExecutionEngineResult(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    return ExecutionEngineResult(TRI_ERROR_INTERNAL);
  }

  // This deactivates the defered cleanup.
  // From here on we rely on the AQL shutdown mechanism.
  guard.cancel();
  return ExecutionEngineResult(query->engine());
}
