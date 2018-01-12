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

#include "Aql/BasicBlocks.h"
#include "Aql/CalculationBlock.h"
#include "Aql/ClusterBlocks.h"
#include "Aql/CollectBlock.h"
#include "Aql/CollectNode.h"
#include "Aql/CollectOptions.h"
#include "Aql/Collection.h"
#include "Aql/EngineInfoContainerCoordinator.h"
#include "Aql/EngineInfoContainerDBServer.h"
#include "Aql/EnumerateCollectionBlock.h"
#include "Aql/EnumerateListBlock.h"
#include "Aql/ExecutionNode.h"
#include "Aql/IndexBlock.h"
#include "Aql/ModificationBlocks.h"
#include "Aql/Query.h"
#include "Aql/QueryRegistry.h"
#include "Aql/ShortestPathBlock.h"
#include "Aql/ShortestPathNode.h"
#include "Aql/SortBlock.h"
#include "Aql/SubqueryBlock.h"
#include "Aql/TraversalBlock.h"
#include "Aql/WalkerWorker.h"
#include "Basics/Exceptions.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/CollectionLockState.h"
#include "Cluster/TraverserEngineRegistry.h"
#include "Logger/Logger.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Methods.h"
#include "VocBase/ticks.h"

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

    // We need to traverse the nodes from back to front, the walker collects them
    // in the wrong ordering
    for (auto it = nodes.rbegin(); it != nodes.rend(); ++it) {
      auto en = *it;
      auto const nodeType = en->getType();

      if (nodeType == ExecutionNode::REMOTE) {
        remoteNode = static_cast<RemoteNode const*>(en);
        continue;
      }

      // for all node types but REMOTEs, we create blocks
      std::unique_ptr<ExecutionBlock> uptrEb(createBlock(en, cache, includedShards));

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

/// @brief helper function to create a block
ExecutionBlock* ExecutionEngine::createBlock(
    ExecutionNode const* en,
    std::unordered_map<ExecutionNode*, ExecutionBlock*> const& cache,
    std::unordered_set<std::string> const& includedShards) {
  switch (en->getType()) {
    case ExecutionNode::SINGLETON: {
      return new SingletonBlock(this, static_cast<SingletonNode const*>(en));
    }
    case ExecutionNode::INDEX: {
      return new IndexBlock(this, static_cast<IndexNode const*>(en));
    }
    case ExecutionNode::ENUMERATE_COLLECTION: {
      return new EnumerateCollectionBlock(
          this, static_cast<EnumerateCollectionNode const*>(en));
    }
    case ExecutionNode::ENUMERATE_LIST: {
      return new EnumerateListBlock(this,
                                    static_cast<EnumerateListNode const*>(en));
    }
    case ExecutionNode::TRAVERSAL: {
      return new TraversalBlock(this, static_cast<TraversalNode const*>(en));
    }
    case ExecutionNode::SHORTEST_PATH: {
      return new ShortestPathBlock(this,
                                   static_cast<ShortestPathNode const*>(en));
    }
    case ExecutionNode::CALCULATION: {
      return new CalculationBlock(this,
                                  static_cast<CalculationNode const*>(en));
    }
    case ExecutionNode::FILTER: {
      return new FilterBlock(this, static_cast<FilterNode const*>(en));
    }
    case ExecutionNode::LIMIT: {
      return new LimitBlock(this, static_cast<LimitNode const*>(en));
    }
    case ExecutionNode::SORT: {
      return new SortBlock(this, static_cast<SortNode const*>(en));
    }
    case ExecutionNode::COLLECT: {
      auto aggregationMethod =
          static_cast<CollectNode const*>(en)->aggregationMethod();

      if (aggregationMethod ==
          CollectOptions::CollectMethod::COLLECT_METHOD_HASH) {
        return new HashedCollectBlock(this,
                                      static_cast<CollectNode const*>(en));
      } else if (aggregationMethod ==
                 CollectOptions::CollectMethod::COLLECT_METHOD_SORTED) {
        return new SortedCollectBlock(this,
                                      static_cast<CollectNode const*>(en));
      } else if (aggregationMethod ==
                 CollectOptions::CollectMethod::COLLECT_METHOD_DISTINCT) {
        return new DistinctCollectBlock(this,
                                      static_cast<CollectNode const*>(en));
      }

      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "cannot instantiate CollectBlock with "
                                     "undetermined aggregation method");
    }
    case ExecutionNode::SUBQUERY: {
      auto es = static_cast<SubqueryNode const*>(en);
      auto it = cache.find(es->getSubquery());

      TRI_ASSERT(it != cache.end());

      return new SubqueryBlock(this, static_cast<SubqueryNode const*>(en),
                               it->second);
    }
    case ExecutionNode::RETURN: {
      return new ReturnBlock(this, static_cast<ReturnNode const*>(en));
    }
    case ExecutionNode::REMOVE: {
      return new RemoveBlock(this, static_cast<RemoveNode const*>(en));
    }
    case ExecutionNode::INSERT: {
      return new InsertBlock(this, static_cast<InsertNode const*>(en));
    }
    case ExecutionNode::UPDATE: {
      return new UpdateBlock(this, static_cast<UpdateNode const*>(en));
    }
    case ExecutionNode::REPLACE: {
      return new ReplaceBlock(this, static_cast<ReplaceNode const*>(en));
    }
    case ExecutionNode::UPSERT: {
      return new UpsertBlock(this, static_cast<UpsertNode const*>(en));
    }
    case ExecutionNode::NORESULTS: {
      return new NoResultsBlock(this, static_cast<NoResultsNode const*>(en));
    }
    case ExecutionNode::SCATTER: {
      auto shardIds =
          static_cast<ScatterNode const*>(en)->collection()->shardIds(
              includedShards);
      return new ScatterBlock(this, static_cast<ScatterNode const*>(en),
                              *shardIds);
    }
    case ExecutionNode::DISTRIBUTE: {
      auto shardIds =
          static_cast<DistributeNode const*>(en)->collection()->shardIds(
              includedShards);
      return new DistributeBlock(
          this, static_cast<DistributeNode const*>(en), *shardIds,
          static_cast<DistributeNode const*>(en)->collection());
    }
    case ExecutionNode::GATHER: {
      return new GatherBlock(this, static_cast<GatherNode const*>(en));
    }
    case ExecutionNode::REMOTE: {
      auto remote = static_cast<RemoteNode const*>(en);
      return new RemoteBlock(this, remote, remote->server(),
                             remote->ownName(), remote->queryId());
    }
  }

  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "illegal node type");
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

      std::unique_ptr<ExecutionBlock> eb(engine->createBlock(
          en, cache, std::unordered_set<std::string>()));

      // do we need to adjust the root node?
      auto const nodeType = en->getType();

      if (nodeType == ExecutionNode::DISTRIBUTE ||
          nodeType == ExecutionNode::SCATTER ||
          nodeType == ExecutionNode::GATHER) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_INTERNAL, "logic error, got cluster node in local query");
      }

      engine->addBlock(eb.get());

      if (!en->hasParent()) {
        // yes. found a new root!
        root = eb.get();
      }

      block = eb.release();
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

 public:
  CoordinatorInstanciator() : _isCoordinator(true), _lastClosed(0) {}

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
          _dbserverParts.addGraphNode(static_cast<GraphNode*>(en));
          break;
        default:
          // Do nothing
          break;
      }

      if (nodeType == ExecutionNode::REMOTE) {
      }
    } else {
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
  ///        * Creates one Query-Entry with all locking information per DBServer
  ///        * Creates one Query-Entry for each Snippet per Shard (multiple on
  ///        the same DB)
  ///        * Snippets DO NOT lock anything, locking is done in the overall
  ///        query.
  ///        * After this step DBServer-Collections are locked!
  ///
  ///        Returns the First Coordinator Engine, the one not in the registry.
  ExecutionEngine* buildEngines(Query* query, QueryRegistry* registry,
                                std::unordered_set<ShardID>* lockedShards) {
    // QueryIds are filled by responses of DBServer parts.
    std::unordered_map<std::string, std::string> queryIds;

    _dbserverParts.buildEngines(query, queryIds, query->queryOptions().shardIds,
                                lockedShards);

    // The coordinator engines cannot decide on lock issues later on,
    // however every engine gets injected the list of locked shards.
    return _coordinatorParts.buildEngines(
        query, registry, query->vocbase()->name(),
        query->queryOptions().shardIds, queryIds, lockedShards);
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
        std::unique_ptr<std::unordered_set<std::string>> lockedShards;
        if (CollectionLockState::_noLockHeaders != nullptr) {
          lockedShards = std::make_unique<std::unordered_set<std::string>>(
              *CollectionLockState::_noLockHeaders);
        } else {
          lockedShards = std::make_unique<std::unordered_set<std::string>>();
        }

        auto inst = std::make_unique<CoordinatorInstanciator>();

        // TODO optionally restrict query to certain shards
        // inst->includedShards(query->queryOptions().shardIds);

        plan->root()->walk(inst.get());

        engine = inst->buildEngines(query, queryRegistry, lockedShards.get());
        // Every engine has copied the list of locked shards anyways. Simply
        // throw this list away.
        // TODO: We can save exactly one copy of this list. Or we could
        // potentially replace it by
        // a single shared_ptr and save the copy all along...

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
      plan->root()->walk(inst.get());
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
