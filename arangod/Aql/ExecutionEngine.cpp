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

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/BlocksWithClients.h"
#include "Aql/Collection.h"
#include "Aql/AqlItemBlockManager.h"
#include "Aql/EngineInfoContainerCoordinator.h"
#include "Aql/EngineInfoContainerDBServerServerBased.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/GraphNode.h"
#include "Aql/IdExecutor.h"
#include "Aql/OptimizerRule.h"
#include "Aql/QueryContext.h"
#include "Aql/RemoteExecutor.h"
#include "Aql/ReturnExecutor.h"
#include "Aql/SkipResult.h"
#include "Aql/SharedQueryState.h"
#include "Aql/WalkerWorker.h"
#include "Basics/ScopeGuard.h"
#include "Cluster/ServerState.h"
#include "Futures/Utilities.h"
#include "Logger/LogMacros.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "Network/Utils.h"

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

  ~TraverserEngineShardLists() = default;

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
 * @param queryIds A Mapping: RemoteNodeId -> DBServerId -> [snippetId]
 *
 * @return A result containing the error in bad case.
 */
Result ExecutionEngine::createBlocks(std::vector<ExecutionNode*> const& nodes,
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
      continue; // handled on GatherNode
    }

    // for all node types but REMOTEs, we create blocks
    auto uptrEb = en->createBlock(*this, cache);

    if (!uptrEb) {
      return {TRI_ERROR_INTERNAL, "illegal node type"};
    }

    // transfers ownership
    // store the pointer to the block
    auto eb = addBlock(std::move(uptrEb));

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
          remoteNode->setDistributeId({""});
          std::unique_ptr<ExecutionBlock> r = remoteNode->createBlock(*this, {});
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
          auto remoteBlock = dynamic_cast<ExecutionBlockImpl<RemoteExecutor>*>(r.get());
          TRI_ASSERT(remoteBlock->server() == serverID);
          TRI_ASSERT(remoteBlock->distributeId() == "");  // NOLINT(readability-container-size-empty)
          TRI_ASSERT(remoteBlock->queryId() == snippetId);
#endif

          TRI_ASSERT(r != nullptr);
          eb->addDependency(r.get());
          addBlock(std::move(r));
        }
      }
    }

    // the last block is always the root
    root(eb);

    // put it into our cache:
    cache.try_emplace(en, eb);
  }
  return {TRI_ERROR_NO_ERROR};
}

/// @brief create the engine
ExecutionEngine::ExecutionEngine(QueryContext& query, AqlItemBlockManager& itemBlockMgr,
                                 SerializationFormat format,
                                 std::shared_ptr<SharedQueryState> sqs)
    : _query(query),
      _itemBlockManager(itemBlockMgr),
      _sharedState((sqs != nullptr)
                       ? std::move(sqs)
                       : std::make_shared<SharedQueryState>(query.vocbase().server())),
      _blocks(),
      _root(nullptr),
      _resultRegister(0),
      _initializeCursorCalled(false),
      _shutdownState(ShutdownState::NotShutdown) {
  TRI_ASSERT(_sharedState != nullptr);
  _blocks.reserve(8);
}

/// @brief destroy the engine, frees all assigned blocks
ExecutionEngine::~ExecutionEngine() {
  try {
    shutdownSync(TRI_ERROR_INTERNAL);
  } catch (...) {
    // shutdown can throw - ignore it in the destructor
  }
  
  if (_sharedState) {  // ensure no async task is working anymore
    _sharedState->invalidate();
  }

  for (auto& it : _blocks) {
    delete it;
  }
}

struct SingleServerQueryInstanciator final : public WalkerWorker<ExecutionNode> {
  ExecutionEngine& engine;
  ExecutionBlock* root{};
  std::unordered_map<ExecutionNode*, ExecutionBlock*> cache;

  explicit SingleServerQueryInstanciator(ExecutionEngine& engine) noexcept
      : engine(engine) {}
        
  void after(ExecutionNode* en) override {
    if (en->getType() == ExecutionNode::TRAVERSAL ||
        en->getType() == ExecutionNode::SHORTEST_PATH ||
        en->getType() == ExecutionNode::K_SHORTEST_PATHS) {
      // We have to prepare the options before we build the block
      ExecutionNode::castTo<GraphNode*>(en)->prepareOptions();
    }

    ExecutionBlock* block = nullptr;
    if (!arangodb::ServerState::instance()->isDBServer()) {
      auto const nodeType = en->getType();

      if (nodeType == ExecutionNode::DISTRIBUTE ||
          nodeType == ExecutionNode::SCATTER ||
          (nodeType == ExecutionNode::GATHER &&
           // simon: parallel traversals use a GatherNode
           static_cast<GatherNode*>(en)->parallelism() != GatherNode::Parallelism::Parallel)) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_INTERNAL,
            "logic error, got cluster node in local query");
      }
    } else {
      auto const& cached = cache.find(en);
      if (cached != cache.end()) {
        block = cached->second;
        TRI_ASSERT(block != nullptr);
      }
    }

    if (block == nullptr) {
      block = engine.addBlock(en->createBlock(engine, cache));
      TRI_ASSERT(block != nullptr);
      // We have visited this node earlier, so we got its dependencies
      // Now add dependencies:
      for (auto const& it : en->getDependencies()) {
        auto it2 = cache.find(it);
        TRI_ASSERT(it2 != cache.end());
        TRI_ASSERT(it2->second != nullptr);
        block->addDependency(it2->second);
      }

      cache.try_emplace(en, block);
    }
    TRI_ASSERT(block != nullptr);
      
    // do we need to adjust the root node?
    if (!en->hasParent()) {
      // yes. found a new root!
      root = block;
    }
  }

  // Override this method for DBServers, there it is now possible to visit the same block twice
  bool done(ExecutionNode* en) override { return false; }
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

struct DistributedQueryInstanciator final : public WalkerWorker<ExecutionNode> {
 private:
  EngineInfoContainerCoordinator _coordinatorParts;
  EngineInfoContainerDBServerServerBased _dbserverParts;
  bool _isCoordinator;
  bool const _pushToSingleServer;
  QueryId _lastClosed;
  Query& _query;
  // This is a handle to the last gather node that we see while traversing the
  // plan The guarantee is that we only have the combination `Remote <- Gather
  // <- before` Therefore we will always assert that this is NULLPTR with the
  // only exception of this case.
  GatherNode const* _lastGatherNode;
  std::unordered_map<ExecutionNodeId, ExecutionNode*> const& _nodesById;

 public:
  DistributedQueryInstanciator(Query& query,
                               std::unordered_map<ExecutionNodeId, ExecutionNode*> const& nodesById,
                               bool pushToSingleServer)
      : _dbserverParts(query),
        _isCoordinator(true),
        _pushToSingleServer(pushToSingleServer),
        _lastClosed(0),
        _query(query),
        _lastGatherNode(nullptr),
        _nodesById(nodesById) {}

  /// @brief before method for collection of pieces phase
  ///        Collects all nodes on the path and divides them
  ///        into coordinator and dbserver parts
  bool before(ExecutionNode* en) final {
    auto const nodeType = en->getType();
    if (_isCoordinator) {
      _coordinatorParts.addNode(en);

      switch (nodeType) {
        case ExecutionNode::GATHER:
          _lastGatherNode = ExecutionNode::castTo<GatherNode const*>(en);
          break;
        case ExecutionNode::REMOTE:
          // Flip over to DBServer
          _isCoordinator = false;
          TRI_ASSERT(_lastGatherNode != nullptr);
          _dbserverParts.openSnippet(_lastGatherNode, en->id());
          _lastGatherNode = nullptr;
          break;
        case ExecutionNode::TRAVERSAL:
        case ExecutionNode::SHORTEST_PATH:
        case ExecutionNode::K_SHORTEST_PATHS:
          _dbserverParts.addGraphNode(ExecutionNode::castTo<GraphNode*>(en), _pushToSingleServer);
          break;
        default:
          // Do nothing
          break;
      }
      // lastGatherNode <=> nodeType is gather
      TRI_ASSERT((_lastGatherNode != nullptr) == (nodeType == ExecutionNode::GATHER));
    } else {
      // on dbserver
      _dbserverParts.addNode(en, _pushToSingleServer);
      // switch back from DB server to coordinator, if we are not pushing the
      // entire plan to the DB server
      if (ExecutionNode::REMOTE == nodeType) {
        TRI_ASSERT(!_pushToSingleServer);
        _isCoordinator = true;
        _coordinatorParts.openSnippet(en->id());
      }
    }

    // Always return false to not abort searching
    return false;
  }

  void after(ExecutionNode* en) final {
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
  Result buildEngines(AqlItemBlockManager& mgr, SnippetList& snippets) {
    TRI_ASSERT(ServerState::instance()->isCoordinator());
    
    // QueryIds are filled by responses of DBServer parts.
    MapRemoteToSnippet snippetIds{};

    std::map<std::string, QueryId> serverToQueryId;
    std::map<ExecutionNodeId, ExecutionNodeId> nodeAliases;
    Result res =
        _dbserverParts.buildEngines(_nodesById, snippetIds, serverToQueryId, nodeAliases);
    if (res.fail()) {
      return res;
    }

    // The coordinator engines cannot decide on lock issues later on,
    // however every engine gets injected the list of locked shards.
    res = _coordinatorParts.buildEngines(_query, mgr, snippetIds, snippets);
    if (res.fail()) {
      return res;
    }
    
    TRI_ASSERT(snippets.size() > 0);
    TRI_ASSERT(snippets[0].first == 0);
    
    bool knowsAllQueryIds = snippetIds.empty() || !serverToQueryId.empty();
    // simon: pre 3.7 servers do not have queryIds, might not have all IDs
    for (auto const& [serverDst, queryId] : serverToQueryId) {
      if (queryId == 0) {
        knowsAllQueryIds = false;
        break;
      }
    }
    
    for (auto& [engineId, engine] : snippets) {
      if (knowsAllQueryIds) {
        if (engineId != 0) { // only the root snippet executes a shutdown on the Coordinator
          engine->setShutdown(ExecutionEngine::ShutdownState::Done);
        }
      } else {
        engine->setShutdown(ExecutionEngine::ShutdownState::Legacy);
      }
    }
    
    TRI_ASSERT(snippets[0].first == 0);
    snippets[0].second->snippetMapping(std::move(snippetIds), std::move(serverToQueryId));
    snippets[0].second->globalStats().setAliases(std::move(nodeAliases));
    TRI_ASSERT(snippets[0].second->shutdownState() != ExecutionEngine::ShutdownState::Done);
    
    return res;
  }
};

void ExecutionEngine::kill() {
  if (!ServerState::instance()->isCoordinator()) {
    TRI_ASSERT(_dbServerMapping.empty());
    return;
  }

  // just use the global query shutdown instead of killing
  if (shutdownState() != ShutdownState::Legacy) {
    shutdown(TRI_ERROR_QUERY_KILLED);
    return;
  }
  
  // kill DB server parts
  // RemoteNodeId -> DBServerId -> [snippetId]
  NetworkFeature const& nf = _query.vocbase().server().getFeature<NetworkFeature>();
  network::ConnectionPool* pool = nf.pool();
  if (pool == nullptr) {
    return;
  }
  
  network::RequestOptions options;
  options.database = _query.vocbase().name();
  options.timeout = network::Timeout(30.0);
  
  // TODO: maybe kill global DBServers ?
  VPackBuffer<uint8_t> body;
  std::vector<network::FutureRes> futures;

  for (auto const& it : _dbServerMapping) {
    for (auto const& it2 : it.second) {
      for (auto const& snippetId : it2.second) {
        TRI_ASSERT(it2.first.substr(0, 7) == "server:");
        auto future = network::sendRequest(pool, it2.first, fuerte::RestVerb::Delete,
                                           "/_api/aql/kill/" + snippetId, body, options);
        futures.emplace_back(std::move(future));
      }
    }
  }

  if (!futures.empty()) {
    // killing is best-effort
    // we are ignoring all errors intentionally here
    futures::collectAll(futures).get();
  }
}

std::pair<ExecutionState, Result> ExecutionEngine::initializeCursor(SharedAqlItemBlockPtr&& items,
                                                                    size_t pos) {
  if (_query.killed()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_KILLED);
  }
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

auto ExecutionEngine::execute(AqlCallStack const& stack)
    -> std::tuple<ExecutionState, SkipResult, SharedAqlItemBlockPtr> {
  if (_query.killed()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_KILLED);
  }
  auto const res = _root->execute(stack);
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  if (std::get<ExecutionState>(res) == ExecutionState::WAITING) {
    auto const skipped = std::get<SkipResult>(res);
    auto const block = std::get<SharedAqlItemBlockPtr>(res);
    TRI_ASSERT(skipped.nothingSkipped());
    TRI_ASSERT(block == nullptr);
  }
#endif
  return res;
}

auto ExecutionEngine::executeForClient(AqlCallStack const& stack, std::string const& clientId)
    -> std::tuple<ExecutionState, SkipResult, SharedAqlItemBlockPtr> {
  if (_query.killed()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_KILLED);
  }

  auto rootBlock = dynamic_cast<BlocksWithClients*>(root());
  if (rootBlock == nullptr) {
    using namespace std::string_literals;
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL_AQL,
                                   "unexpected node type "s +
                                       root()->getPlanNode()->getTypeString());
  }

  auto const res = rootBlock->executeForClient(stack, clientId);
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  if (std::get<ExecutionState>(res) == ExecutionState::WAITING) {
    auto const skipped = std::get<SkipResult>(res);
    auto const& block = std::get<SharedAqlItemBlockPtr>(res);
    TRI_ASSERT(skipped.nothingSkipped());
    TRI_ASSERT(block == nullptr);
  }
#endif
  return res;
}

std::pair<ExecutionState, SharedAqlItemBlockPtr> ExecutionEngine::getSome(size_t atMost) {
  if (_query.killed()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_KILLED);
  }
  if (!_initializeCursorCalled) {
    auto res = initializeCursor(nullptr, 0);
    if (res.first == ExecutionState::WAITING) {
      return {res.first, nullptr};
    }
  }
  // we use a backwards compatible stack here.
  // This will always continue with a fetch-all on underlying subqueries (if any)
  AqlCallStack compatibilityStack{AqlCallList{AqlCall::SimulateGetSome(atMost)}, true};
  auto const [state, skipped, block] = _root->execute(std::move(compatibilityStack));
  // We cannot trigger a skip operation from here
  TRI_ASSERT(skipped.nothingSkipped());
  return {state, std::move(block)};
}

std::pair<ExecutionState, size_t> ExecutionEngine::skipSome(size_t atMost) {
  if (_query.killed()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_KILLED);
  }
  if (!_initializeCursorCalled) {
    auto res = initializeCursor(nullptr, 0);
    if (res.first == ExecutionState::WAITING) {
      return {res.first, 0};
    }
  }

  // we use a backwards compatible stack here.
  // This will always continue with a fetch-all on underlying subqueries (if any)
  AqlCallStack compatibilityStack{AqlCallList{AqlCall::SimulateSkipSome(atMost)}, true};
  auto const [state, skipped, block] = _root->execute(std::move(compatibilityStack));
  // We cannot be triggered within a subquery from earlier versions.
  // Also we cannot produce anything ourselfes here.
  TRI_ASSERT(block == nullptr);
  return {state, skipped.getSkipCount()};
}

Result ExecutionEngine::shutdownSync(int errorCode) noexcept try {
  if (_root == nullptr || shutdownState() == ShutdownState::Done) {
    return Result();
  }
  
  Result res{TRI_ERROR_INTERNAL, "unable to shutdown query"};
  ExecutionState state = ExecutionState::WAITING;
  try {
    TRI_IF_FAILURE("ExecutionEngine::shutdownSync") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    std::shared_ptr<SharedQueryState> sharedState = _sharedState;
    TRI_ASSERT(sharedState != nullptr);
    sharedState->resetWakeupHandler();
    while (state == ExecutionState::WAITING) {
      std::tie(state, res) = shutdown(errorCode);
      if (state == ExecutionState::WAITING) {
        sharedState->waitForAsyncWakeup();
      }
    }
  } catch (basics::Exception const& ex) {
    res.reset(ex.code(), std::string("unable to shutdown query: ") + ex.what());
  } catch (std::exception const& ex) {
    res.reset(TRI_ERROR_INTERNAL, std::string("unable to shutdown query: ") + ex.what());
  } catch (...) {
    res.reset(TRI_ERROR_INTERNAL);
  }

  return res;
  
} catch (...) {
  // nothing we can do here...
  return Result(TRI_ERROR_INTERNAL, "unable to shutdown query");
}

/// @brief shutdown, will be called exactly once for the whole query
std::pair<ExecutionState, Result> ExecutionEngine::shutdown(int errorCode) {
  auto st = _shutdownState.load(std::memory_order_relaxed);
  if (_root == nullptr || st == ShutdownState::Done) {
    return {ExecutionState::DONE, _shutdownResult};
  }
  
  TRI_ASSERT(_sharedState != nullptr);
  
  if (ShutdownState::NotShutdown == st) {
    if (_shutdownState.compare_exchange_strong(st, ShutdownState::ShutdownSent, std::memory_order_relaxed)) {
      // enter shutdown phase, forget previous wakeups
      _sharedState->resetNumWakeups();
      return shutdownDBServerQueries(errorCode);
    }
    TRI_ASSERT(st == ShutdownState::ShutdownSent);
  }
  
  if (st == ShutdownState::ShutdownSent) {
    return {ExecutionState::WAITING, {}};
  } else if (ShutdownState::Legacy == st) {
    // enter shutdown phase, forget previous wakeups
    _sharedState->resetNumWakeups();
    
    auto [state, res] = _root->shutdown(errorCode);
    if (state == ExecutionState::WAITING) {
      return {state, res};
    }
    
    LOG_TOPIC("47b85", INFO, Logger::QUERIES) << "executing legacy query shutdown";
    
    _shutdownState.store(ShutdownState::Done, std::memory_order_relaxed);
    
    return {state, res};
  }
  
  return {ExecutionState::DONE, _shutdownResult};
}

// @brief create an execution engine from a plan
void ExecutionEngine::instantiateFromPlan(Query& query,
                                          ExecutionPlan& plan,
                                          bool planRegisters,
                                          SerializationFormat format,
                                          SnippetList& snippets) {
  auto const role = arangodb::ServerState::instance()->getRole();
  
  TRI_ASSERT(snippets.empty() || ServerState::instance()->isClusterRole(role));

  plan.findVarUsage();
  if (planRegisters) {
    plan.planRegisters();
    plan.findCollectionAccessVariables();
  }

  ExecutionBlock* root = nullptr;
  ExecutionEngine* engine = nullptr;
#ifdef USE_ENTERPRISE
  bool const pushToSingleServer = plan.hasAppliedRule(
      static_cast<int>(OptimizerRule::RuleLevel::clusterOneShardRule));
#else
  bool const pushToSingleServer = false;
#endif
  
  AqlItemBlockManager& mgr = query.itemBlockManager();

  if (arangodb::ServerState::isCoordinator(role)) {
    // distributed query
    DistributedQueryInstanciator inst(query, plan.getNodesById(), pushToSingleServer);
    plan.root()->walk(inst);

    Result res = inst.buildEngines(mgr, snippets);
    if (res.fail()) {
      THROW_ARANGO_EXCEPTION(res);
    }
    
    TRI_ASSERT(snippets.size() > 0);
    TRI_ASSERT(snippets[0].first == 0);
    engine = snippets[0].second.get();
    root = snippets[0].second->root();
    
  } else {
    
#ifdef USE_ENTERPRISE
    std::map<aql::ExecutionNodeId, aql::ExecutionNodeId> aliases;
    ExecutionEngine::parallelizeTraversals(query, plan, aliases);
#endif
   
    // instantiate the engine on a local server
    auto retEngine = std::make_unique<ExecutionEngine>(query, mgr, format, query.sharedState());
    
#ifdef USE_ENTERPRISE
    for (auto const& pair : aliases) {
      retEngine->_execStats.addAlias(pair.first, pair.second);
    }
#endif

    SingleServerQueryInstanciator inst(*retEngine);
    plan.root()->walk(inst);

    root = inst.root;
    engine = retEngine.get();

    snippets.emplace_back(std::make_pair(0, std::move(retEngine)));
  }

  TRI_ASSERT(root != nullptr);

  // inspect the root block of the query
  if (root->getPlanNode()->getType() == ExecutionNode::RETURN) {
    // it's a return node. now tell it to not copy its results from above,
    // but directly return it. we also need to note the RegisterId the
    // caller needs to look into when fetching the results

    // in short: this avoids copying the return values

    bool const returnInheritedResults =
        ExecutionNode::castTo<ReturnNode const*>(root->getPlanNode())->returnInheritedResults();
    if (returnInheritedResults) {
      auto returnNode =
          dynamic_cast<ExecutionBlockImpl<IdExecutor<SingleRowFetcher<BlockPassthrough::Enable>>>*>(
              root);
      TRI_ASSERT(returnNode != nullptr);
      engine->resultRegister(returnNode->getOutputRegisterId());
    } else {
      auto returnNode = dynamic_cast<ExecutionBlockImpl<ReturnExecutor>*>(root);
      TRI_ASSERT(returnNode != nullptr);
    }
  }

  engine->_root = root; // simon: otherwise it breaks
      
  TRI_ASSERT(snippets.size() == 1 || ServerState::instance()->isClusterRole(role));
}

/// @brief add a block to the engine
ExecutionBlock* ExecutionEngine::addBlock(std::unique_ptr<ExecutionBlock> block) {
  TRI_ASSERT(block != nullptr);

  // TODO track resource usage
  _blocks.emplace_back(block.get());
  return block.release();
}

ExecutionBlock* ExecutionEngine::root() const {
  TRI_ASSERT(_root != nullptr);
  return _root;
}

void ExecutionEngine::root(ExecutionBlock* root) {
  TRI_ASSERT(root != nullptr);
  _root = root;
}

QueryContext& ExecutionEngine::getQuery() const { return _query; }

bool ExecutionEngine::initializeCursorCalled() const {
  return _initializeCursorCalled;
}

void ExecutionEngine::resultRegister(RegisterId resultRegister) {
  _resultRegister = resultRegister;
}

RegisterId ExecutionEngine::resultRegister() const { return _resultRegister; }

AqlItemBlockManager& ExecutionEngine::itemBlockManager() {
  return _itemBlockManager;
}

///  @brief collected execution stats
void ExecutionEngine::collectExecutionStats(ExecutionStats& stats) {
  for (ExecutionBlock* block : _blocks) {
    block->collectExecStats(_execStats);
  }
  stats.add(_execStats);
  _execStats.clear();
}

std::pair<ExecutionState, Result> ExecutionEngine::shutdownDBServerQueries(int errorCode) {
  TRI_ASSERT(shutdownState() == ShutdownState::ShutdownSent);
  if (_serverToQueryId.empty()) { // happens during tests
    _shutdownState.store(ShutdownState::Done);
    return {ExecutionState::DONE, Result()};
  }
  
  NetworkFeature const& nf = _query.vocbase().server().getFeature<NetworkFeature>();
  network::ConnectionPool* pool = nf.pool();
  if (pool == nullptr) {
    return {ExecutionState::DONE, Result(TRI_ERROR_SHUTTING_DOWN)};
  }
  
  network::RequestOptions options;
  options.database = _query.vocbase().name();
  options.timeout = network::Timeout(60.0);  // Picked arbitrarily

  VPackBuffer<uint8_t> body;
  VPackBuilder builder(body);
  builder.openObject(true);
  builder.add(StaticStrings::Code, VPackValue(errorCode));
  builder.close();
  
  _query.incHttpRequests(static_cast<unsigned>(_serverToQueryId.size()));
   
  std::vector<futures::Future<futures::Unit>> futures;
  futures.reserve(_serverToQueryId.size());
  auto ss = _sharedState;
  TRI_ASSERT(ss != nullptr);

  for (auto const& [serverDst, queryId] : _serverToQueryId) {

    TRI_ASSERT(serverDst.substr(0, 7) == "server:");
    
    auto f = network::sendRequest(pool, serverDst, fuerte::RestVerb::Delete,
                         "/_api/aql/finish/" + std::to_string(queryId), body, options)
    .thenValue([ss, this](network::Response&& res) {
      ss->executeLocked([&] {
        if (res.fail()) {
          _shutdownResult = network::fuerteToArangoErrorCode(res);
          return;
        } else if (!res.slice().isObject()) {
          _shutdownResult.reset(TRI_ERROR_INTERNAL, "shutdown response is malformed");
          return;
        }
        
        VPackSlice stats = res.slice().get("stats");
        if (stats.isObject()) {
          _execStats.add(ExecutionStats(stats));
        }
        // read "warnings" attribute if present and add it to our query
        VPackSlice warnings = res.slice().get("warnings");
        if (warnings.isArray()) {
          for (VPackSlice it : VPackArrayIterator(warnings)) {
            if (it.isObject()) {
              VPackSlice code = it.get("code");
              VPackSlice message = it.get("message");
              if (code.isNumber() && message.isString()) {
                _query.warnings().registerWarning(code.getNumericValue<int>(),
                                                  message.copyString());
              }
            }
          }
        }
        if (res.slice().hasKey("code") && _shutdownResult.ok()) {
          _shutdownResult.reset(res.slice().get("code").getNumericValue<int>());
        }
      });
    }).thenError<std::exception>([ss, this](std::exception ptr) {
      ss->executeLocked([&] {
        _shutdownResult.reset(TRI_ERROR_INTERNAL, "unhandled query shutdown exception");
      });
    });

    futures.emplace_back(std::move(f));
  }
  
  
  futures::collectAll(std::move(futures)).thenFinal([ss, this](auto&& vals) {
    ss->executeAndWakeup([&] {
      TRI_ASSERT(shutdownState() == ShutdownState::ShutdownSent);
      _shutdownState.store(ShutdownState::Done, std::memory_order_relaxed);
      return true;
    });
  });
  
  return {ExecutionState::WAITING, Result()};
}

#ifndef USE_ENTERPRISE
bool ExecutionEngine::waitForSatellites(QueryContext& /*query*/, Collection const* /*collection*/) const {
  return true;
}
#endif
