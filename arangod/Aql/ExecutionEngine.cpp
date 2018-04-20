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

#include "Aql/AqlResult.h"
#include "Aql/BasicBlocks.h"
#include "Aql/ClusterBlocks.h"
#include "Aql/Collection.h"
#include "Aql/EngineInfoContainerCoordinator.h"
#include "Aql/EngineInfoContainerDBServer.h"
#include "Aql/ExecutionNode.h"
#include "Aql/GraphNode.h"
#include "Aql/Query.h"
#include "Aql/QueryRegistry.h"
#include "Aql/WalkerWorker.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/CollectionLockState.h"
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

void ExecutionEngine::createBlocks(
    std::vector<ExecutionNode*> const& nodes,
    std::unordered_set<std::string> const& includedShards,
    std::unordered_set<std::string> const& restrictToShards,
    std::unordered_map<std::string, std::string> const& queryIds) {
  if (arangodb::ServerState::instance()->isCoordinator()) {
    auto clusterInfo = arangodb::ClusterInfo::instance();

    std::unordered_map<ExecutionNode*, ExecutionBlock*> cache;
    RemoteNode const* remoteNode = nullptr;

    // We need to traverse the nodes from back to front, the walker collects
    // them
    // in the wrong ordering
    for (auto it = nodes.rbegin(); it != nodes.rend(); ++it) {
      auto en = *it;
      auto const nodeType = en->getType();

      if (nodeType == ExecutionNode::REMOTE) {
        remoteNode = static_cast<RemoteNode const*>(en);
        continue;
      }

      // for all node types but REMOTEs, we create blocks
      std::unique_ptr<ExecutionBlock> uptrEb(
          en->createBlock(*this, cache, includedShards));

      if (uptrEb == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "illegal node type");
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
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                         "expecting a remoteNode");
        }

        // now we'll create a remote node for each shard and add it to the
        // gather node
        auto gatherNode = static_cast<GatherNode const*>(en);
        Collection const* collection = gatherNode->collection();

        auto shardIds = collection->shardIds(restrictToShards);
        for (auto const& shardId : *shardIds) {
          std::string theId =
              arangodb::basics::StringUtils::itoa(remoteNode->id()) + ":" +
              shardId;

          auto it = queryIds.find(theId);
          if (it == queryIds.end()) {
            THROW_ARANGO_EXCEPTION_MESSAGE(
                TRI_ERROR_INTERNAL,
                "could not find query id " + theId + " in list");
          }

          auto serverList = clusterInfo->getResponsibleServer(shardId);
          if (serverList->empty()) {
            THROW_ARANGO_EXCEPTION_MESSAGE(
                TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE,
                "Could not find responsible server for shard " + shardId);
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
          TRI_ASSERT(!serverList->empty());
          auto& leader = (*serverList)[0];
          auto r = std::make_unique<RemoteBlock>(this, remoteNode,
                                                 "server:" + leader,  // server
                                                 "",                  // ownName
                                                 it->second);         // queryId

          TRI_ASSERT(r != nullptr);

          eb->addDependency(r.get());
          addBlock(r.get());
          r.release();
        }
      }

      // the last block is always the root
      root(eb);

      // put it into our cache:
      cache.emplace(en, eb);
    }
  }
}

/// @brief create the engine
ExecutionEngine::ExecutionEngine(Query* query)
    : _stats(),
      _itemBlockManager(query->resourceMonitor()),
      _blocks(),
      _root(nullptr),
      _query(query),
      _resultRegister(0),
      _wasShutdown(false),
      _previouslyLockedShards(nullptr),
      _lockedShards(nullptr) {
  _blocks.reserve(8);
}

/// @brief destroy the engine, frees all assigned blocks
ExecutionEngine::~ExecutionEngine() {
  try {
    shutdown(TRI_ERROR_INTERNAL);
  } catch (...) {
    // shutdown can throw - ignore it in the destructor
  }

  for (auto& it : _blocks) {
    delete it;
  }
}

struct Instanciator final : public WalkerWorker<ExecutionNode> {
  ExecutionEngine* engine;
  ExecutionBlock* root;
  std::unordered_map<ExecutionNode*, ExecutionBlock*> cache;

  explicit Instanciator(ExecutionEngine* engine)
      : engine(engine), root(nullptr) {}

  ~Instanciator() {}

  virtual void after(ExecutionNode* en) override final {
    ExecutionBlock* block = nullptr;
    {
      if (en->getType() == ExecutionNode::TRAVERSAL ||
          en->getType() == ExecutionNode::SHORTEST_PATH) {
        // We have to prepare the options before we build the block
        static_cast<GraphNode*>(en)->prepareOptions();
      }

      // do we need to adjust the root node?
      auto const nodeType = en->getType();

      if (nodeType == ExecutionNode::DISTRIBUTE ||
          nodeType == ExecutionNode::SCATTER ||
          nodeType == ExecutionNode::GATHER) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_INTERNAL, "logic error, got cluster node in local query");
      }

      static const std::unordered_set<std::string> EMPTY;
      block = engine->addBlock(en->createBlock(*engine, cache, EMPTY));

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

struct CoordinatorInstanciator : public WalkerWorker<ExecutionNode> {
 private:
  EngineInfoContainerCoordinator _coordinatorParts;
  EngineInfoContainerDBServer _dbserverParts;
  bool _isCoordinator;
  QueryId _lastClosed;
  Query* _query;

 public:
  CoordinatorInstanciator(Query* query)
      : _isCoordinator(true), _lastClosed(0), _query(query) {}

  ~CoordinatorInstanciator() {}

  /// @brief before method for collection of pieces phase
  ///        Collects all nodes on the path and devides them
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
          _dbserverParts.addGraphNode(_query, static_cast<GraphNode*>(en));
          break;
        default:
          // Do nothing
          break;
      }
    } else {
      // on dbserver
      _dbserverParts.addNode(en);
      if (nodeType == ExecutionNode::REMOTE) {
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
  ExecutionEngineResult buildEngines(
      QueryRegistry* registry, std::unordered_set<ShardID>& lockedShards) {
    // QueryIds are filled by responses of DBServer parts.
    std::unordered_map<std::string, std::string> queryIds;

    bool needsErrorCleanup = true;
    auto cleanup = [&]() {
      if (needsErrorCleanup) {
        _dbserverParts.cleanupEngines(ClusterComm::instance(),
                                      TRI_ERROR_INTERNAL,
                                      _query->vocbase()->name(), queryIds);
      }
    };
    TRI_DEFER(cleanup());

    _dbserverParts.buildEngines(_query, queryIds,
                                _query->queryOptions().shardIds, lockedShards);

    // The coordinator engines cannot decide on lock issues later on,
    // however every engine gets injected the list of locked shards.
    auto res = _coordinatorParts.buildEngines(
        _query, registry, _query->vocbase()->name(),
        _query->queryOptions().shardIds, queryIds, lockedShards);
    if (res.ok()) {
      needsErrorCleanup = false;
    }
    return res;
  }
};

/// @brief shutdown, will be called exactly once for the whole query
int ExecutionEngine::shutdown(int errorCode) {
  int res = TRI_ERROR_NO_ERROR;
  if (_root != nullptr && !_wasShutdown) {
    // Take care of locking prevention measures in the cluster:
    if (_lockedShards != nullptr) {
      if (CollectionLockState::_noLockHeaders == _lockedShards) {
        CollectionLockState::_noLockHeaders = _previouslyLockedShards;
      }

      delete _lockedShards;
      _lockedShards = nullptr;
      _previouslyLockedShards = nullptr;
    }

    res = _root->shutdown(errorCode);

    // prevent a duplicate shutdown
    _wasShutdown = true;
  }

  return res;
}

/// @brief create an execution engine from a plan
ExecutionEngine* ExecutionEngine::instantiateFromPlan(
    QueryRegistry* queryRegistry, Query* query, ExecutionPlan* plan,
    bool planRegisters) {
  auto role = arangodb::ServerState::instance()->getRole();
  bool const isCoordinator =
      arangodb::ServerState::instance()->isCoordinator(role);
  bool const isDBServer = arangodb::ServerState::instance()->isDBServer(role);

  TRI_ASSERT(queryRegistry != nullptr);

  ExecutionEngine* engine = nullptr;

  try {
    if (!plan->varUsageComputed()) {
      plan->findVarUsage();
    }
    if (planRegisters) {
      plan->planRegisters();
    }

    ExecutionBlock* root = nullptr;

    if (isCoordinator) {
      try {
        std::unordered_set<std::string> lockedShards;
        if (CollectionLockState::_noLockHeaders != nullptr) {
          lockedShards = *CollectionLockState::_noLockHeaders;
        }

        CoordinatorInstanciator inst(query);

        plan->root()->walk(inst);

        auto result = inst.buildEngines(queryRegistry, lockedShards);
        if (!result.ok()) {
          THROW_ARANGO_EXCEPTION_MESSAGE(result.errorNumber(),
                                         result.errorMessage());
        }
        // Every engine has copied the list of locked shards anyways. Simply
        // throw this list away.
        // TODO: We can save exactly one copy of this list. Or we could
        // potentially replace it by
        // a single shared_ptr and save the copy all along...

        engine = result.engine();
        TRI_ASSERT(engine != nullptr);

        // We can always use the _noLockHeaders. They have not been modified
        // until now
        // And it is correct to set the previously locked to nullptr if the
        // headers are nullptr.
        engine->_previouslyLockedShards = CollectionLockState::_noLockHeaders;

        // Now update _noLockHeaders;
        CollectionLockState::_noLockHeaders = engine->_lockedShards;

        root = engine->root();
        TRI_ASSERT(root != nullptr);

      } catch (std::exception const& e) {
        LOG_TOPIC(ERR, Logger::AQL)
            << "Coordinator query instantiation failed: " << e.what();
        throw e;
      } catch (...) {
        throw;
      }
    } else {
      // instantiate the engine on a local server
      engine = new ExecutionEngine(query); 
      auto inst = std::make_unique<Instanciator>(engine); 
      plan->root()->walk(*inst); 
      root = inst.get()->root; 
      TRI_ASSERT(root != nullptr);
    }

    TRI_ASSERT(root != nullptr);

    // inspect the root block of the query
    if (!isDBServer &&
        root->getPlanNode()->getType() == ExecutionNode::RETURN) {
      // it's a return node. now tell it to not copy its results from above,
      // but directly return it. we also need to note the RegisterId the
      // caller needs to look into when fetching the results

      // in short: this avoids copying the return values
      engine->resultRegister(
          static_cast<ReturnBlock*>(root)->returnInheritedResults());
    }

    engine->_root = root;

    if (plan->isResponsibleForInitialize()) {
      root->initialize();
      root->initializeCursor(nullptr, 0);
    }

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
ExecutionBlock* ExecutionEngine::addBlock(
    std::unique_ptr<ExecutionBlock>&& block) {
  TRI_ASSERT(block != nullptr);

  _blocks.emplace_back(block.get());
  return block.release();
}
