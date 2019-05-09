////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "ExecutionEngine.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/AqlResult.h"
#include "Aql/BlocksWithClients.h"
#include "Aql/Collection.h"
#include "Aql/EngineInfoContainerCoordinator.h"
#include "Aql/EngineInfoContainerDBServer.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionNode.h"
#include "Aql/GraphNode.h"
#include "Aql/Query.h"
#include "Aql/QueryRegistry.h"
#include "Aql/RemoteExecutor.h"
#include "Aql/ReturnExecutor.h"
#include "Aql/WalkerWorker.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ServerState.h"
#include "Logger/Logger.h"

using namespace arangodb;
using namespace arangodb::aql;

// @brief Local struct to create the
// information required to build traverser engines
// on DB servers.
struct TraverserEngineShardLists {
  explicit TraverserEngineShardLists(size_t length) {
    // Make sure they all have a fixed size.
    edgeCollections.resize(length);
  }

  ~TraverserEngineShardLists() {}

  // Mapping for edge collections to shardIds.
  // We have to retain the ordering of edge collections, all
  // vectors of these in one run need to have identical size.
  // This is because the conditions to query those edges have the
  // same ordering.
  std::vector<std::vector<ShardID>> edgeCollections;

  // Mapping for vertexCollections to shardIds.
  std::unordered_map<std::string, std::vector<ShardID>> vertexCollections;

#ifdef USE_ENTERPRISE
  std::set<ShardID> inaccessibleShards;
#endif
};

/**
 * @brief Create AQL blocks from a list of ExectionNodes
 * Only works in cluster mode
 *
 * @param nodes The list of Nodes => Blocks
 * @param restrictToShards This query is restricted to those shards
 * @param queryIds A Mapping: RemoteNodeId -> DBServerId -> [snippetId]
 *
 * @return A result containing the error in bad case.
 */
Result ExecutionEngine::createBlocks(std::vector<ExecutionNode*> const& nodes,
                                     std::unordered_set<std::string> const& restrictToShards,
                                     MapRemoteToSnippet const& queryIds) {
  TRI_ASSERT(arangodb::ServerState::instance()->isCoordinator());

  std::unordered_map<ExecutionNode*, ExecutionBlock*> cache;
  RemoteNode* remoteNode = nullptr;

  // We need to traverse the nodes from back to front, the walker collects
  // them in the wrong ordering
  for (auto it = nodes.rbegin(); it != nodes.rend(); ++it) {
    auto en = *it;
    auto const nodeType = en->getType();

    if (nodeType == ExecutionNode::REMOTE) {
      remoteNode = ExecutionNode::castTo<RemoteNode*>(en);
      continue;
    }

    // for all node types but REMOTEs, we create blocks
    auto uptrEb = en->createBlock(*this, cache);

    if (!uptrEb) {
      return {TRI_ERROR_INTERNAL, "illegal node type"};
    }

    auto eb = uptrEb.get();

    // Transfers ownership
    addBlock(eb);
    uptrEb.release();

    for (auto const& dep : en->getDependencies()) {
      auto d = cache.find(dep);

      if (d != cache.end()) {
        // add regular dependencies
        TRI_ASSERT((*d).second != nullptr);
        eb->addDependency((*d).second);
      }
    }

    if (nodeType == ExecutionNode::GATHER) {
      // we found a gather node
      if (remoteNode == nullptr) {
        return {TRI_ERROR_INTERNAL, "expecting a RemoteNode"};
      }

      // now we'll create a remote node for each shard and add it to the
      // gather node (eb->addDependency)
      auto serversForRemote = queryIds.find(remoteNode->id());
      // Planning gone terribly wrong. The RemoteNode does not have a
      // counter-part to fetch data from.
      TRI_ASSERT(serversForRemote != queryIds.end());
      if (serversForRemote == queryIds.end()) {
        return {TRI_ERROR_INTERNAL,
                "Did not find a DBServer to contact for RemoteNode"};
      }

      // use "server:" instead of "shard:" to send query fragments to
      // the correct servers, even after failover or when a follower drops
      // the problem with using the previous shard-based approach was that
      // responsibilities for shards may change at runtime.
      // however, an AQL query must send all requests for the query to the
      // initially used servers.
      // if there is a failover while the query is executing, we must still
      // send all following requests to the same servers, and not the newly
      // responsible servers.
      // otherwise we potentially would try to get data from a query from
      // server B while the query was only instanciated on server A.
      for (auto const& serverToSnippet : serversForRemote->second) {
        std::string const& serverID = serverToSnippet.first;
        for (std::string const& snippetId : serverToSnippet.second) {
          remoteNode->queryId(snippetId);
          remoteNode->server(serverID);
          remoteNode->ownName({""});
          std::unique_ptr<ExecutionBlock> r = remoteNode->createBlock(*this, {});
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
          auto remoteBlock = dynamic_cast<ExecutionBlockImpl<RemoteExecutor>*>(r.get());
          TRI_ASSERT(remoteBlock->server() == serverID);
          TRI_ASSERT(remoteBlock->ownName() == "");  // NOLINT(readability-container-size-empty)
          TRI_ASSERT(remoteBlock->queryId() == snippetId);
#endif

          TRI_ASSERT(r != nullptr);
          eb->addDependency(r.get());
          addBlock(r.get());
          r.release();
        }
      }
    }

    // the last block is always the root
    root(eb);

    // put it into our cache:
    cache.emplace(en, eb);
  }
  return {TRI_ERROR_NO_ERROR};
}

/// @brief create the engine
ExecutionEngine::ExecutionEngine(Query* query)
    : _stats(),
      _itemBlockManager(query->resourceMonitor()),
      _blocks(),
      _root(nullptr),
      _query(query),
      _resultRegister(0),
      _initializeCursorCalled(false),
      _wasShutdown(false) {
  _blocks.reserve(8);
}

/// @brief destroy the engine, frees all assigned blocks
ExecutionEngine::~ExecutionEngine() {
  try {
    shutdownSync(TRI_ERROR_INTERNAL);
  } catch (...) {
    // shutdown can throw - ignore it in the destructor
  }

  for (auto& it : _blocks) {
    delete it;
  }
}

struct Instanciator final : public WalkerWorker<ExecutionNode> {
  ExecutionEngine* engine;
  ExecutionBlock* root{};
  std::unordered_map<ExecutionNode*, ExecutionBlock*> cache;

  explicit Instanciator(ExecutionEngine* engine) noexcept : engine(engine) {}

  virtual void after(ExecutionNode* en) override final {
    ExecutionBlock* block = nullptr;
    {
      if (en->getType() == ExecutionNode::TRAVERSAL ||
          en->getType() == ExecutionNode::SHORTEST_PATH ||
          en->getType() == ExecutionNode::K_SHORTEST_PATHS) {
        // We have to prepare the options before we build the block
        ExecutionNode::castTo<GraphNode*>(en)->prepareOptions();
      }

      // do we need to adjust the root node?
      auto const nodeType = en->getType();

      if (nodeType == ExecutionNode::DISTRIBUTE ||
          nodeType == ExecutionNode::SCATTER || nodeType == ExecutionNode::GATHER) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_INTERNAL, "logic error, got cluster node in local query");
      }

      block = engine->addBlock(en->createBlock(*engine, cache));

      if (!en->hasParent()) {
        // yes. found a new root!
        root = block;
      }
    }

    TRI_ASSERT(block != nullptr);

    // Now add dependencies:
    for (auto const& it : en->getDependencies()) {
      auto it2 = cache.find(it);
      TRI_ASSERT(it2 != cache.end());
      TRI_ASSERT(it2->second != nullptr);
      block->addDependency(it2->second);
    }

    cache.emplace(en, block);
  }
};

// Here is a description of how the instantiation of an execution plan
// works in the cluster. See below for a complete example
//
// The instantiation of this works as follows:
// (0) Variable usage and register planning is done in the global plan
// (1) A walk with subqueries is done on the whole plan
//     The purpose is to plan how many ExecutionEngines we need, where they
//     have to be instantiated and which plan nodes belong to each of them.
//     Such a walk is depth first and visits subqueries after it has visited
//     the dependencies of the subquery node recursively. Whenever the
//     walk passes by a RemoteNode it switches location between coordinator
//     and DBserver and starts a new engine. The nodes of an engine are
//     collected in the after method.
//     This walk results in a list of engines and a list of nodes for
//     each engine. It follows that the order in these lists is as follows:
//     The first engine is the main one on the coordinator, it has id 0.
//     The order of the engines is exactly as they are discovered in the
//     walk. That is, engines closer to the root are earlier and engines
//     in subqueries are later. The nodes in each engine are always
//     done in a way such that a dependency D of a node N is earlier in the
//     list as N, and a subquery node is later in the list than the nodes
//     of the subquery.
// (2) buildEngines is called with that data. It proceeds engine by engine,
//     starting from the back of the list. This means that an engine that
//     is referred to in a RemoteNode (because its nodes are dependencies
//     of that node) are always already instantiated before the RemoteNode
//     is instantiated. The corresponding query ids are collected in a
//     global hash table, for which the key consists of the id of the
//     RemoteNode using the query and the actual query id. For each engine,
//     the nodes are instantiated along the list of nodes for that engine.
//     This means that all dependencies of a node N are already instantiated
//     when N is instantiated. We distinguish the coordinator and the
//     DBserver case. In the former one we have to clone a part of the
//     plan and in the latter we have to send a part to a DBserver via HTTP.
//
// Here is a fully worked out example:
//
// FOR i IN [1,2]
//   FOR d IN coll
//     FILTER d.pass == i
//     LET s = (FOR e IN coll2 FILTER e.name == d.name RETURN e)
//     RETURN {d:d, s:s}
//
// this is optimized to, variable and register planning is done in this plan:
//
//    Singleton
//        ^
//   EnumList [1,2]             Singleton
//        ^                         ^
//     Scatter (2)            Enum coll2
//        ^                         ^
//     Remote              Calc e.name==d.name
//        ^                         ^
//    Enum coll                  Filter (3)
//        ^                         ^
//  Calc d.pass==i               Remote
//        ^                         ^
//     Filter (1)                Gather
//        ^                         ^
//     Remote                    Return
//        ^                         ^
//     Gather                       |
//        ^                         |
//     Subquery  -------------------/
//        ^
//  Calc {d:d, s:s}
//        ^
//      Return (0)
//
// There are 4 engines here, their corresponding root nodes are labelled
// in the above picture in round brackets with the ids of the engine.
// Engines 1 and 3 have to be replicated for each shard of coll or coll2
// respectively, and sent to the right DBserver via HTTP. Engine 0 is the
// main one on the coordinator and engine 2 is a non-main part on the
// coordinator. Recall that the walk goes first to the dependencies before
// it visits the nodes of the subquery. Thus, the walk builds up the lists
// in this order:
//   engine 0: [Remote, Gather, Remote, Gather, Return, Subquery, Calc, Return]
//   engine 1: [Remote, Enum coll, Calc d.pass==i, Filter]
//   engine 2: [Singleton, EnumList [1,2], Scatter]
//   engine 3: [Singleton, Enum coll2, Calc e.name==d.name, Filter]
// buildEngines will then do engines in the order 3, 2, 1, 0 and for each
// of them the nodes from left to right in these lists. In the end, we have
// a proper instantiation of the whole thing.

struct CoordinatorInstanciator final : public WalkerWorker<ExecutionNode> {
 private:
  EngineInfoContainerCoordinator _coordinatorParts;
  EngineInfoContainerDBServer _dbserverParts;
  bool _isCoordinator;
  QueryId _lastClosed;
  Query* _query;

 public:
  explicit CoordinatorInstanciator(Query* query) noexcept
      : _dbserverParts(query), _isCoordinator(true), _lastClosed(0), _query(query) {
    TRI_ASSERT(_query);
  }

  /// @brief before method for collection of pieces phase
  ///        Collects all nodes on the path and divides them
  ///        into coordinator and dbserver parts
  bool before(ExecutionNode* en) override final {
    auto const nodeType = en->getType();
    if (_isCoordinator) {
      _coordinatorParts.addNode(en);

      switch (nodeType) {
        case ExecutionNode::REMOTE:
          // Flip over to DBServer
          _isCoordinator = false;
          _dbserverParts.openSnippet(en->id());
          break;
        case ExecutionNode::TRAVERSAL:
        case ExecutionNode::SHORTEST_PATH:
        case ExecutionNode::K_SHORTEST_PATHS:
          _dbserverParts.addGraphNode(ExecutionNode::castTo<GraphNode*>(en));
          break;
        default:
          // Do nothing
          break;
      }
    } else {
      // on dbserver
      _dbserverParts.addNode(en);
      if (ExecutionNode::REMOTE == nodeType) {
        _isCoordinator = true;
        _coordinatorParts.openSnippet(en->id());
      }
    }

    // Always return false to not abort searching
    return false;
  }

  void after(ExecutionNode* en) override final {
    if (en->getType() == ExecutionNode::REMOTE) {
      if (_isCoordinator) {
        _lastClosed = _coordinatorParts.closeSnippet();
        _isCoordinator = false;
      } else {
        _dbserverParts.closeSnippet(_lastClosed);
        _isCoordinator = true;
      }
    }
  }

  /// @brief Builds the Engines necessary for the query execution
  ///        For Coordinator Parts:
  ///        * Creates the ExecutionBlocks
  ///        * Injects all Parts but the First one into QueryRegistery
  ///        For DBServer Parts
  ///        * Creates one Query-Entry for each Snippet per Shard (multiple on
  ///        the same DB) Each Snippet knows all details about locking.
  ///        * Only the first snippet does lock the collections.
  ///        other snippets are not responsible for any locking.
  ///        * After this step DBServer-Collections are locked!
  ///
  ///        Error Case:
  ///        * It is guaranteed that all DBServers will be send a request
  ///        to remove query snippets / locks they have locally created.
  ///        * No Engines for this query will remain in the Coordinator
  ///        Registry.
  ///        * In case the Network is broken, all non-reachable DBServers will
  ///        clean out their snippets after a TTL.
  ///        Returns the First Coordinator Engine, the one not in the registry.
  ExecutionEngineResult buildEngines(QueryRegistry* registry) {
    // QueryIds are filled by responses of DBServer parts.
    MapRemoteToSnippet queryIds{};

    auto cleanupGuard = scopeGuard([this, &queryIds]() {
      _dbserverParts.cleanupEngines(ClusterComm::instance(), TRI_ERROR_INTERNAL,
                                    _query->vocbase().name(), queryIds);
    });

    ExecutionEngineResult res = _dbserverParts.buildEngines(queryIds);
    if (res.fail()) {
      return res;
    }

    // The coordinator engines cannot decide on lock issues later on,
    // however every engine gets injected the list of locked shards.
    res = _coordinatorParts.buildEngines(_query, registry, _query->vocbase().name(),
                                         _query->queryOptions().shardIds, queryIds);

    if (res.ok()) {
      cleanupGuard.cancel();
    }

    return res;
  }
};

std::pair<ExecutionState, Result> ExecutionEngine::initializeCursor(SharedAqlItemBlockPtr&& items,
                                                                    size_t pos) {
  InputAqlItemRow inputRow{CreateInvalidInputRowHint{}};
  if (items != nullptr) {
    inputRow = InputAqlItemRow{std::move(items), pos};
  }
  auto res = _root->initializeCursor(inputRow);
  if (res.first == ExecutionState::WAITING) {
    return res;
  }
  _initializeCursorCalled = true;
  return res;
}

std::pair<ExecutionState, SharedAqlItemBlockPtr> ExecutionEngine::getSome(size_t atMost) {
  if (!_initializeCursorCalled) {
    auto res = initializeCursor(nullptr, 0);
    if (res.first == ExecutionState::WAITING) {
      return {res.first, nullptr};
    }
  }
  return _root->getSome(atMost);
}

std::pair<ExecutionState, size_t> ExecutionEngine::skipSome(size_t atMost) {
  if (!_initializeCursorCalled) {
    auto res = initializeCursor(nullptr, 0);
    if (res.first == ExecutionState::WAITING) {
      return {res.first, 0};
    }
  }
  return _root->skipSome(atMost);
}

Result ExecutionEngine::shutdownSync(int errorCode) noexcept {
  Result res{TRI_ERROR_INTERNAL};
  ExecutionState state = ExecutionState::WAITING;
  try {
    std::shared_ptr<SharedQueryState> sharedState = _query->sharedState();
    if (sharedState != nullptr) {
      sharedState->setContinueCallback();

      while (state == ExecutionState::WAITING) {
        std::tie(state, res) = shutdown(errorCode);
        if (state == ExecutionState::WAITING) {
          sharedState->waitForAsyncResponse();
        }
      }
    }
  } catch (...) {
    res.reset(TRI_ERROR_INTERNAL);
  }
  return res;
}

/// @brief shutdown, will be called exactly once for the whole query
std::pair<ExecutionState, Result> ExecutionEngine::shutdown(int errorCode) {
  ExecutionState state = ExecutionState::DONE;
  Result res;
  if (_root != nullptr && !_wasShutdown) {
    std::tie(state, res) = _root->shutdown(errorCode);
    if (state == ExecutionState::WAITING) {
      return {state, res};
    }

    // prevent a duplicate shutdown
    _wasShutdown = true;
  }

  return {state, res};
}

/// @brief create an execution engine from a plan
ExecutionEngine* ExecutionEngine::instantiateFromPlan(QueryRegistry* queryRegistry,
                                                      Query* query, ExecutionPlan* plan,
                                                      bool planRegisters) {
  auto role = arangodb::ServerState::instance()->getRole();
  bool const isCoordinator = arangodb::ServerState::isCoordinator(role);
  bool const isDBServer = arangodb::ServerState::isDBServer(role);

  TRI_ASSERT(queryRegistry != nullptr);

  ExecutionEngine* engine = nullptr;

  try {
    plan->findVarUsage();
    if (planRegisters) {
      plan->planRegisters();
    }

    ExecutionBlock* root = nullptr;

    if (isCoordinator) {
      try {
        CoordinatorInstanciator inst(query);

        plan->root()->walk(inst);

        auto result = inst.buildEngines(queryRegistry);
        if (!result.ok()) {
          THROW_ARANGO_EXCEPTION_MESSAGE(result.errorNumber(), result.errorMessage());
        }
        // Every engine has copied the list of locked shards anyways. Simply
        // throw this list away.
        // TODO: We can save exactly one copy of this list. Or we could
        // potentially replace it by
        // a single shared_ptr and save the copy all along...

        engine = result.engine();
        TRI_ASSERT(engine != nullptr);

        root = engine->root();
        TRI_ASSERT(root != nullptr);

      } catch (std::exception const& e) {
        LOG_TOPIC("bc9d5", ERR, Logger::AQL)
            << "Coordinator query instantiation failed: " << e.what();
        throw;
      }
    } else {
      // instantiate the engine on a local server
      engine = new ExecutionEngine(query);
      Instanciator inst(engine);
      plan->root()->walk(inst);
      root = inst.root;
      TRI_ASSERT(root != nullptr);
    }

    TRI_ASSERT(root != nullptr);

    // inspect the root block of the query
    if (root->getPlanNode()->getType() == ExecutionNode::RETURN) {
      // it's a return node. now tell it to not copy its results from above,
      // but directly return it. we also need to note the RegisterId the
      // caller needs to look into when fetching the results

      // in short: this avoids copying the return values

      bool const returnInheritedResults = !isDBServer;
      if (returnInheritedResults) {
        auto returnNode = dynamic_cast<ExecutionBlockImpl<ReturnExecutor<true>>*>(root);
        TRI_ASSERT(returnNode != nullptr);
        engine->resultRegister(returnNode->infos().getInputRegisterId());
        TRI_ASSERT(returnNode->infos().returnInheritedResults() == returnInheritedResults);
      } else {
        auto returnNode = dynamic_cast<ExecutionBlockImpl<ReturnExecutor<false>>*>(root);
        TRI_ASSERT(returnNode != nullptr);
        TRI_ASSERT(returnNode->infos().returnInheritedResults() == returnInheritedResults);
      }
    }

    engine->_root = root;

    return engine;
  } catch (...) {
    if (!isCoordinator) {
      delete engine;
    }
    throw;
  }
}

/// @brief add a block to the engine
void ExecutionEngine::addBlock(ExecutionBlock* block) {
  TRI_ASSERT(block != nullptr);

  _blocks.emplace_back(block);
}

/// @brief add a block to the engine
ExecutionBlock* ExecutionEngine::addBlock(std::unique_ptr<ExecutionBlock>&& block) {
  TRI_ASSERT(block != nullptr);

  _blocks.emplace_back(block.get());
  return block.release();
}
