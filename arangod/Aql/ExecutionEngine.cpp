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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "Aql/ExecutionEngine.h"
#include "Aql/CollectOptions.h"
#include "Aql/BasicBlocks.h"
#include "Aql/CalculationBlock.h"
#include "Aql/ClusterBlocks.h"
#include "Aql/CollectBlock.h"
#include "Aql/CollectNode.h"
#include "Aql/EnumerateCollectionBlock.h"
#include "Aql/EnumerateListBlock.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/IndexBlock.h"
#include "Aql/ModificationBlocks.h"
#include "Aql/QueryRegistry.h"
#include "Aql/SortBlock.h"
#include "Aql/SubqueryBlock.h"
#include "Aql/TraversalBlock.h"
#include "Aql/WalkerWorker.h"
#include "Basics/Exceptions.h"
#include "Basics/Logger.h"
#include "Cluster/ClusterComm.h"
#include "VocBase/server.h"

using namespace arangodb::aql;

using Json = arangodb::basics::Json;

////////////////////////////////////////////////////////////////////////////////
/// @brief helper function to create a block
////////////////////////////////////////////////////////////////////////////////

static ExecutionBlock* CreateBlock(
    ExecutionEngine* engine, ExecutionNode const* en,
    std::unordered_map<ExecutionNode*, ExecutionBlock*> const& cache) {
  switch (en->getType()) {
    case ExecutionNode::SINGLETON: {
      return new SingletonBlock(engine, static_cast<SingletonNode const*>(en));
    }
    case ExecutionNode::INDEX: {
      return new IndexBlock(engine, static_cast<IndexNode const*>(en));
    }
    case ExecutionNode::ENUMERATE_COLLECTION: {
      return new EnumerateCollectionBlock(
          engine, static_cast<EnumerateCollectionNode const*>(en));
    }
    case ExecutionNode::ENUMERATE_LIST: {
      return new EnumerateListBlock(engine,
                                    static_cast<EnumerateListNode const*>(en));
    }
    case ExecutionNode::TRAVERSAL: {
      return new TraversalBlock(engine, static_cast<TraversalNode const*>(en));
    }
    case ExecutionNode::CALCULATION: {
      return new CalculationBlock(engine,
                                  static_cast<CalculationNode const*>(en));
    }
    case ExecutionNode::FILTER: {
      return new FilterBlock(engine, static_cast<FilterNode const*>(en));
    }
    case ExecutionNode::LIMIT: {
      return new LimitBlock(engine, static_cast<LimitNode const*>(en));
    }
    case ExecutionNode::SORT: {
      return new SortBlock(engine, static_cast<SortNode const*>(en));
    }
    case ExecutionNode::COLLECT: {
      auto aggregationMethod =
          static_cast<CollectNode const*>(en)->aggregationMethod();

      if (aggregationMethod ==
          CollectOptions::CollectMethod::COLLECT_METHOD_HASH) {
        return new HashedCollectBlock(engine,
                                      static_cast<CollectNode const*>(en));
      } else if (aggregationMethod ==
                 CollectOptions::CollectMethod::COLLECT_METHOD_SORTED) {
        return new SortedCollectBlock(engine,
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

      return new SubqueryBlock(engine, static_cast<SubqueryNode const*>(en),
                               it->second);
    }
    case ExecutionNode::RETURN: {
      return new ReturnBlock(engine, static_cast<ReturnNode const*>(en));
    }
    case ExecutionNode::REMOVE: {
      return new RemoveBlock(engine, static_cast<RemoveNode const*>(en));
    }
    case ExecutionNode::INSERT: {
      return new InsertBlock(engine, static_cast<InsertNode const*>(en));
    }
    case ExecutionNode::UPDATE: {
      return new UpdateBlock(engine, static_cast<UpdateNode const*>(en));
    }
    case ExecutionNode::REPLACE: {
      return new ReplaceBlock(engine, static_cast<ReplaceNode const*>(en));
    }
    case ExecutionNode::UPSERT: {
      return new UpsertBlock(engine, static_cast<UpsertNode const*>(en));
    }
    case ExecutionNode::NORESULTS: {
      return new NoResultsBlock(engine, static_cast<NoResultsNode const*>(en));
    }
    case ExecutionNode::SCATTER: {
      auto shardIds =
          static_cast<ScatterNode const*>(en)->collection()->shardIds();
      return new ScatterBlock(engine, static_cast<ScatterNode const*>(en),
                              *shardIds);
    }
    case ExecutionNode::DISTRIBUTE: {
      auto shardIds =
          static_cast<DistributeNode const*>(en)->collection()->shardIds();
      return new DistributeBlock(
          engine, static_cast<DistributeNode const*>(en), *shardIds,
          static_cast<DistributeNode const*>(en)->collection());
    }
    case ExecutionNode::GATHER: {
      return new GatherBlock(engine, static_cast<GatherNode const*>(en));
    }
    case ExecutionNode::REMOTE: {
      auto remote = static_cast<RemoteNode const*>(en);
      return new RemoteBlock(engine, remote, remote->server(),
                             remote->ownName(), remote->queryId());
    }
    case ExecutionNode::ILLEGAL: {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "illegal node type");
    }
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create the engine
////////////////////////////////////////////////////////////////////////////////

ExecutionEngine::ExecutionEngine(Query* query)
    : _stats(),
      _blocks(),
      _root(nullptr),
      _query(query),
      _resultRegister(0),
      _wasShutdown(false),
      _previouslyLockedShards(nullptr),
      _lockedShards(nullptr) {
  _blocks.reserve(8);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the engine, frees all assigned blocks
////////////////////////////////////////////////////////////////////////////////

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
      std::unique_ptr<ExecutionBlock> eb(CreateBlock(engine, en, cache));

      if (eb == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "illegal node type");
      }

      // do we need to adjust the root node?
      auto const nodeType = en->getType();

      if (!en->hasParent()) {
        // yes. found a new root!
        root = eb.get();
      }

      if (nodeType == ExecutionNode::DISTRIBUTE ||
          nodeType == ExecutionNode::SCATTER ||
          nodeType == ExecutionNode::GATHER) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_INTERNAL, "logic error, got cluster node in local query");
      }

      engine->addBlock(eb.get());
      block = eb.release();
    }

    TRI_ASSERT(block != nullptr);

    // Now add dependencies:
    for (auto const& it : en->getDependencies()) {
      auto it2 = cache.find(it);
      TRI_ASSERT(it2 != cache.end());
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
  enum EngineLocation { COORDINATOR, DBSERVER };

  struct EngineInfo {
    EngineInfo(EngineLocation location, size_t id, arangodb::aql::QueryPart p,
               size_t idOfRemoteNode)
        : location(location),
          id(id),
          nodes(),
          part(p),
          idOfRemoteNode(idOfRemoteNode) {}

    Collection* getCollection() const {
      Collection* collection = nullptr;

      for (auto en = nodes.rbegin(); en != nodes.rend(); ++en) {
        // find the collection to be used
        if ((*en)->getType() == ExecutionNode::ENUMERATE_COLLECTION) {
          collection = const_cast<Collection*>(
              static_cast<EnumerateCollectionNode*>((*en))->collection());
        } else if ((*en)->getType() == ExecutionNode::INDEX) {
          collection = const_cast<Collection*>(
              static_cast<IndexNode*>((*en))->collection());
        } else if ((*en)->getType() == ExecutionNode::INSERT ||
                   (*en)->getType() == ExecutionNode::UPDATE ||
                   (*en)->getType() == ExecutionNode::REPLACE ||
                   (*en)->getType() == ExecutionNode::REMOVE ||
                   (*en)->getType() == ExecutionNode::UPSERT) {
          collection = const_cast<Collection*>(
              static_cast<ModificationNode*>((*en))->collection());
        }
      }

      TRI_ASSERT(collection != nullptr);
      return collection;
    }

    EngineLocation const location;
    size_t const id;
    std::vector<ExecutionNode*> nodes;
    arangodb::aql::QueryPart part;  // only relevant for DBserver parts
    size_t idOfRemoteNode;          // id of the remote node
    // in the original plan that needs this engine
  };

  Query* query;
  QueryRegistry* queryRegistry;
  ExecutionBlock* root;
  EngineLocation currentLocation;
  size_t currentEngineId;
  std::vector<EngineInfo> engines;
  std::vector<size_t> engineStack;  // stack of engine ids, used for
                                    // RemoteNodes
  std::unordered_set<std::string> collNamesSeenOnDBServer;
  // names of sharded collections that we have already seen on a DBserver
  // this is relevant to decide whether or not the engine there is a main
  // query or a dependent one.
  std::unordered_map<std::string, std::string> queryIds;
  // this map allows to find the queries which are the parts of the big
  // query. There are two cases, the first is for the remote queries on
  // the DBservers, for these, the key is:
  //   itoa(ID of RemoteNode in original plan) + ":" + shardId
  // and the value is the
  //   queryId on DBserver
  // with a * appended, if it is a PART_MAIN query.
  // The second case is a query, which lives on the coordinator but is not
  // the main query. For these, we store
  //   itoa(ID of RemoteNode in original plan) + "/" + <name of vocbase>
  // and the value is the
  //   queryId used in the QueryRegistry
  // this is built up when we instantiate the various engines on the
  // DBservers and used when we instantiate the ones on the
  // coordinator. Note that the main query and engine is not put into
  // this map at all.

  CoordinatorInstanciator(Query* query, QueryRegistry* queryRegistry)
      : query(query),
        queryRegistry(queryRegistry),
        root(nullptr),
        currentLocation(COORDINATOR),
        currentEngineId(0),
        engines() {
    TRI_ASSERT(query != nullptr);
    TRI_ASSERT(queryRegistry != nullptr);

    engines.emplace_back(COORDINATOR, 0, PART_MAIN, 0);
  }

  ~CoordinatorInstanciator() {}

  //////////////////////////////////////////////////////////////////////////////
  /// @brief generatePlanForOneShard
  //////////////////////////////////////////////////////////////////////////////

  void generatePlanForOneShard(VPackBuilder& builder, size_t nr,
                               EngineInfo const& info, QueryId& connectedId,
                               std::string const& shardId, bool verbose) {
    // copy the relevant fragment of the plan for each shard
    // Note that in these parts of the query there are no SubqueryNodes,
    // since they are all on the coordinator!
    ExecutionPlan plan(query->ast());

    ExecutionNode* previous = nullptr;
    for (ExecutionNode const* current : info.nodes) {
      auto clone = current->clone(&plan, false, false);
      // UNNECESSARY, because clone does it: plan.registerNode(clone);

      if (current->getType() == ExecutionNode::REMOTE) {
        // update the remote node with the information about the query
        static_cast<RemoteNode*>(clone)
            ->server("server:" + arangodb::ServerState::instance()->getId());
        static_cast<RemoteNode*>(clone)->ownName(shardId);
        static_cast<RemoteNode*>(clone)->queryId(connectedId);
        // only one of the remote blocks is responsible for forwarding the
        // initializeCursor and shutDown requests
        // for simplicity, we always use the first remote block if we have more
        // than one
        static_cast<RemoteNode*>(clone)->isResponsibleForInitCursor(nr == 0);
      }

      if (previous != nullptr) {
        clone->addDependency(previous);
      }

      previous = clone;
    }
    plan.root(previous);
    plan.setVarUsageComputed();
    return plan.root()->toVelocyPack(builder, verbose);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief distributePlanToShard, send a single plan to one shard
  //////////////////////////////////////////////////////////////////////////////

  void distributePlanToShard(arangodb::CoordTransactionID& coordTransactionID,
                             EngineInfo const& info, Collection* collection,
                             QueryId& connectedId, std::string const& shardId,
                             TRI_json_t* jsonPlan) {
    // create a JSON representation of the plan
    Json result(Json::Object);

    // inject the current shard id into the collection
    collection->setCurrentShard(shardId);

    Json jsonNodesList(TRI_UNKNOWN_MEM_ZONE, jsonPlan, Json::NOFREE);

    // add the collection
    Json jsonCollectionsList(Json::Array);
    Json json(Json::Object);
    jsonCollectionsList(json("name", Json(collection->getName()))(
        "type", Json(TRI_TransactionTypeGetStr(collection->accessType))));

    jsonNodesList.set("collections", jsonCollectionsList);
    jsonNodesList.set("variables",
                      query->ast()->variables()->toJson(TRI_UNKNOWN_MEM_ZONE));

    result.set("plan", jsonNodesList);
    if (info.part == arangodb::aql::PART_MAIN) {
      result.set("part", Json("main"));
    } else {
      result.set("part", Json("dependent"));
    }

    Json optimizerOptionsRules(Json::Array);
    Json optimizerOptions(Json::Object);

    Json options(Json::Object);
    optimizerOptionsRules.add(Json("-all"));
    optimizerOptions.set("rules", optimizerOptionsRules);
    options.set("optimizer", optimizerOptions);
    result.set("options", options);
    auto body = std::make_shared<std::string const>(
        arangodb::basics::JsonHelper::toString(result.json()));

    // std::cout << "GENERATED A PLAN FOR THE REMOTE SERVERS: " << *(body.get())
    // << "\n";

    auto cc = arangodb::ClusterComm::instance();

    std::string const url("/_db/" + arangodb::basics::StringUtils::urlEncode(
                                        collection->vocbase->_name) +
                          "/_api/aql/instantiate");

    std::unique_ptr<std::map<std::string, std::string>> headers(
        new std::map<std::string, std::string>());
    (*headers)["X-Arango-Nolock"] = shardId;  // Prevent locking
    auto res = cc->asyncRequest("", coordTransactionID, "shard:" + shardId,
                                arangodb::rest::HttpRequest::HTTP_REQUEST_POST,
                                url, body, headers, nullptr, 30.0);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief aggregateQueryIds, get answers for all shards in a Scatter/Gather
  //////////////////////////////////////////////////////////////////////////////

  void aggregateQueryIds(EngineInfo const& info, arangodb::ClusterComm*& cc,
                         arangodb::CoordTransactionID& coordTransactionID,
                         Collection* collection) {
    // pick up the remote query ids
    auto shardIds = collection->shardIds();

    std::string error;
    int count = 0;
    int nrok = 0;
    for (count = (int)shardIds->size(); count > 0; count--) {
      auto res = cc->wait("", coordTransactionID, 0, "", 30.0);

      if (res.status == arangodb::CL_COMM_RECEIVED) {
        if (res.answer_code == arangodb::rest::HttpResponse::OK ||
            res.answer_code == arangodb::rest::HttpResponse::CREATED ||
            res.answer_code == arangodb::rest::HttpResponse::ACCEPTED) {
          // query instantiated without problems
          nrok++;

          // pick up query id from response
          arangodb::basics::Json response(
              TRI_UNKNOWN_MEM_ZONE,
              arangodb::basics::JsonHelper::fromString(res.answer->body()));
          std::string queryId = arangodb::basics::JsonHelper::getStringValue(
              response.json(), "queryId", "");

          // std::cout << "DB SERVER ANSWERED WITHOUT ERROR: " <<
          // res.answer->body() << ", REMOTENODEID: " << info.idOfRemoteNode <<
          // " SHARDID:"  << res.shardID << ", QUERYID: " << queryId << "\n";
          std::string theID =
              arangodb::basics::StringUtils::itoa(info.idOfRemoteNode) + ":" +
              res.shardID;
          if (info.part == arangodb::aql::PART_MAIN) {
            queryIds.emplace(theID, queryId + "*");
          } else {
            queryIds.emplace(theID, queryId);
          }
        } else {
          error += "DB SERVER ANSWERED WITH ERROR: ";
          error += res.answer->body();
          error += "\n";
        }
      } else {
        error += std::string("Communication with shard '") +
                 std::string(res.shardID) + std::string("' on cluster node '") +
                 std::string(res.serverID) + std::string("' failed : ") +
                 res.errorMessage;
      }
    }

    // std::cout << "GOT ALL RESPONSES FROM DB SERVERS: " << nrok << "\n";

    if (nrok != (int)shardIds->size()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, error);
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief distributePlansToShards, for a single Scatter/Gather block
  //////////////////////////////////////////////////////////////////////////////

  void distributePlansToShards(EngineInfo const& info, QueryId connectedId) {
    // std::cout << "distributePlansToShards: " << info.id << std::endl;
    Collection* collection = info.getCollection();
    // now send the plan to the remote servers
    arangodb::CoordTransactionID coordTransactionID = TRI_NewTickServer();
    auto cc = arangodb::ClusterComm::instance();
    TRI_ASSERT(cc != nullptr);

    // iterate over all shards of the collection
    size_t nr = 0;
    auto shardIds = collection->shardIds();
    for (auto const& shardId : *shardIds) {
      // inject the current shard id into the collection
      collection->setCurrentShard(shardId);
      VPackBuilder b;
      generatePlanForOneShard(b, nr++, info, connectedId, shardId, true);

      std::unique_ptr<TRI_json_t> tmp(arangodb::basics::VelocyPackHelper::velocyPackToJson(b.slice()));

      distributePlanToShard(coordTransactionID, info, collection, connectedId,
                            shardId, tmp.release());
    }

    // fix collection
    collection->resetCurrentShard();
    aggregateQueryIds(info, cc, coordTransactionID, collection);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief buildEngineCoordinator, for a single piece
  //////////////////////////////////////////////////////////////////////////////

  ExecutionEngine* buildEngineCoordinator(EngineInfo& info) {
    Query* localQuery = query;
    bool needToClone = info.id > 0;  // use the original for the main part
    if (needToClone) {
      // need a new query instance on the coordinator
      localQuery = query->clone(PART_DEPENDENT, false);
      if (localQuery == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                       "cannot clone query");
      }
    }

    try {
      auto engine = std::make_unique<ExecutionEngine>(localQuery);
      localQuery->engine(engine.get());

      std::unordered_map<ExecutionNode*, ExecutionBlock*> cache;
      RemoteNode* remoteNode = nullptr;

      for (auto en = info.nodes.begin(); en != info.nodes.end(); ++en) {
        auto const nodeType = (*en)->getType();

        if (nodeType == ExecutionNode::REMOTE) {
          remoteNode = static_cast<RemoteNode*>((*en));
          continue;
        }

        // for all node types but REMOTEs, we create blocks
        ExecutionBlock* eb = CreateBlock(engine.get(), (*en), cache);

        if (eb == nullptr) {
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                         "illegal node type");
        }

        try {
          engine.get()->addBlock(eb);
        } catch (...) {
          delete eb;
          throw;
        }

        for (auto const& dep : (*en)->getDependencies()) {
          auto d = cache.find(dep);

          if (d != cache.end()) {
            // add regular dependencies
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
          Collection const* collection =
              static_cast<GatherNode const*>((*en))->collection();

          auto shardIds = collection->shardIds();

          for (auto const& shardId : *shardIds) {
            std::string theId =
                arangodb::basics::StringUtils::itoa(remoteNode->id()) + ":" +
                shardId;
            auto it = queryIds.find(theId);
            if (it == queryIds.end()) {
              THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                             "could not find query id in list");
            }
            std::string idThere = it->second;
            if (idThere.back() == '*') {
              idThere.pop_back();
            }
            ExecutionBlock* r = new RemoteBlock(engine.get(), remoteNode,
                                                "shard:" + shardId,  // server
                                                "",                  // ownName
                                                idThere);            // queryId

            try {
              engine.get()->addBlock(r);
            } catch (...) {
              delete r;
              throw;
            }

            eb->addDependency(r);
          }
        }

        // the last block is always the root
        engine->root(eb);

        // put it into our cache:
        cache.emplace(*en, eb);
      }

      TRI_ASSERT(engine->root() != nullptr);

      // localQuery is stored in the engine
      return engine.release();
    } catch (...) {
      localQuery->engine(nullptr);  // engine is already destroyed by unique_ptr
      if (needToClone) {
        delete localQuery;
      }
      throw;
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief buildEngines, build engines on DBservers and coordinator
  //////////////////////////////////////////////////////////////////////////////

  ExecutionEngine* buildEngines() {
    ExecutionEngine* engine = nullptr;
    QueryId id = 0;

    for (auto it = engines.rbegin(); it != engines.rend(); ++it) {
      // std::cout << "Doing engine: " << it->id << " location:"
      //          << it->location << std::endl;
      if ((*it).location == COORDINATOR) {
        // create a coordinator-based engine
        engine = buildEngineCoordinator(*it);
        TRI_ASSERT(engine != nullptr);

        if ((*it).id > 0) {
          // create a remote id for the engine that we can pass to
          // the plans to be created for the DBServers
          id = TRI_NewTickServer();

          try {
            queryRegistry->insert(id, engine->getQuery(), 3600.0);
          } catch (...) {
            delete engine->getQuery();
            // This deletes the new query as well as the engine
            throw;
          }
          try {
            std::string queryId = arangodb::basics::StringUtils::itoa(id);
            std::string theID =
                arangodb::basics::StringUtils::itoa(it->idOfRemoteNode) + "/" +
                engine->getQuery()->vocbase()->_name;
            queryIds.emplace(theID, queryId);
          } catch (...) {
            queryRegistry->destroy(engine->getQuery()->vocbase(), id,
                                   TRI_ERROR_INTERNAL);
            // This deletes query, engine and entry in QueryRegistry
            throw;
          }
        }
      } else {
        // create an engine on a remote DB server
        // hand in the previous engine's id
        distributePlansToShards((*it), id);
      }
    }

    TRI_ASSERT(engine != nullptr);

    // return the last created coordinator-based engine
    // this is the local engine that we'll use to run the query
    return engine;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief before method for collection of pieces phase
  //////////////////////////////////////////////////////////////////////////////

  virtual bool before(ExecutionNode* en) override final {
    auto const nodeType = en->getType();

    if (nodeType == ExecutionNode::REMOTE) {
      // got a remote node
      // this indicates the end of an execution section

      engineStack.push_back(currentEngineId);

      // begin a new engine
      // flip current location
      currentLocation =
          (currentLocation == COORDINATOR ? DBSERVER : COORDINATOR);
      currentEngineId = engines.size();
      QueryPart part = PART_DEPENDENT;
      if (currentLocation == DBSERVER) {
        auto rn = static_cast<RemoteNode*>(en);
        Collection const* coll = rn->collection();
        if (collNamesSeenOnDBServer.find(coll->name) ==
            collNamesSeenOnDBServer.end()) {
          part = PART_MAIN;
          collNamesSeenOnDBServer.insert(coll->name);
        }
      }
      // For the coordinator we do not care about main or part:
      engines.emplace_back(currentLocation, currentEngineId, part, en->id());
    }

    return false;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief after method for collection of pieces phase
  //////////////////////////////////////////////////////////////////////////////

  virtual void after(ExecutionNode* en) override final {
    auto const nodeType = en->getType();

    if (nodeType == ExecutionNode::REMOTE) {
      currentEngineId = engineStack.back();
      engineStack.pop_back();
      currentLocation = engines[currentEngineId].location;
    }

    // assign the current node to the current engine
    engines[currentEngineId].nodes.emplace_back(en);
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief create an execution engine from a plan
////////////////////////////////////////////////////////////////////////////////

ExecutionEngine* ExecutionEngine::instantiateFromPlan(
    QueryRegistry* queryRegistry, Query* query, ExecutionPlan* plan,
    bool planRegisters) {
  auto role = arangodb::ServerState::instance()->getRole();
  bool const isCoordinator =
      arangodb::ServerState::instance()->isCoordinator(role);
  bool const isDBServer = arangodb::ServerState::instance()->isDBServer(role);

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
      // instantiate the engine on the coordinator

      auto inst =
          std::make_unique<CoordinatorInstanciator>(query, queryRegistry);
      plan->root()->walk(inst.get());

#if 0
      // Just for debugging
      for (auto& ei : inst->engines) {
        std::cout << "EngineInfo: id=" << ei.id 
                  << " Location=" << ei.location << std::endl;
        for (auto& n : ei.nodes) {
          std::cout << "Node: type=" << n->getTypeString() << std::endl;
        }
      }
#endif

      try {
        engine = inst.get()->buildEngines();
        root = engine->root();
        // Now find all shards that take part:
        if (Transaction::_makeNolockHeaders != nullptr) {
          engine->_lockedShards = new std::unordered_set<std::string>(
              *Transaction::_makeNolockHeaders);
          engine->_previouslyLockedShards = Transaction::_makeNolockHeaders;
        } else {
          engine->_lockedShards = new std::unordered_set<std::string>();
          engine->_previouslyLockedShards = nullptr;
        }
        std::map<std::string, std::string> forLocking;
        for (auto& q : inst.get()->queryIds) {
          std::string theId = q.first;
          std::string queryId = q.second;
          // std::cout << "queryIds: " << theId << " : " << queryId <<
          // std::endl;
          auto pos = theId.find(':');
          if (pos != std::string::npos) {
            // So this is a remote one on a DBserver:
            if (queryId.back() == '*') {  // only the PART_MAIN one!
              queryId.pop_back();
              std::string shardId = theId.substr(pos + 1);
              engine->_lockedShards->insert(shardId);
              forLocking.emplace(shardId, queryId);
            }
          }
        }
        // Second round, this time we deal with the coordinator pieces
        // and tell them the lockedShards as well, we need to copy, since
        // they want to delete independently:
        for (auto& q : inst.get()->queryIds) {
          std::string theId = q.first;
          std::string queryId = q.second;
          // std::cout << "queryIds: " << theId << " : " << queryId <<
          // std::endl;
          auto pos = theId.find('/');
          if (pos != std::string::npos) {
            // std::cout << "Setting lockedShards for query ID "
            //          << queryId << std::endl;
            QueryId qId = arangodb::basics::StringUtils::uint64(queryId);
            TRI_vocbase_t* vocbase = query->vocbase();
            Query* q = queryRegistry->open(vocbase, qId);
            q->engine()->setLockedShards(
                new std::unordered_set<std::string>(*engine->_lockedShards));
            queryRegistry->close(vocbase, qId);
            // std::cout << "Setting lockedShards done." << std::endl;
          }
        }
        // Now lock them all in the right order:
        for (auto& p : forLocking) {
          std::string const& shardId = p.first;
          std::string const& queryId = p.second;
          // Lock shard on DBserver:
          arangodb::CoordTransactionID coordTransactionID = TRI_NewTickServer();
          auto cc = arangodb::ClusterComm::instance();
          TRI_vocbase_t* vocbase = query->vocbase();
          std::string const url(
              "/_db/" +
              arangodb::basics::StringUtils::urlEncode(vocbase->_name) +
              "/_api/aql/lock/" + queryId);
          std::map<std::string, std::string> headers;
          auto res =
              cc->syncRequest("", coordTransactionID, "shard:" + shardId,
                              arangodb::rest::HttpRequest::HTTP_REQUEST_PUT,
                              url, "{}", headers, 30.0);
          if (res->status != CL_COMM_SENT) {
            std::string message("could not lock all shards");
            if (res->errorMessage.length() > 0) {
              message += std::string(" : ") + res->errorMessage;
            }
            THROW_ARANGO_EXCEPTION_MESSAGE(
                TRI_ERROR_QUERY_COLLECTION_LOCK_FAILED, message);
          }
        }
        Transaction::_makeNolockHeaders = engine->_lockedShards;
      } catch (...) {
        // We need to destroy all queries that we have built and stuffed
        // into the QueryRegistry as well as those that we have pushed to
        // the DBservers via HTTP:
        TRI_vocbase_t* vocbase = query->vocbase();
        for (auto& q : inst.get()->queryIds) {
          std::string theId = q.first;
          std::string queryId = q.second;
          auto pos = theId.find(':');
          if (pos != std::string::npos) {
            // So this is a remote one on a DBserver:
            std::string shardId = theId.substr(pos + 1);
            // Remove query from DBserver:
            arangodb::CoordTransactionID coordTransactionID =
                TRI_NewTickServer();
            auto cc = arangodb::ClusterComm::instance();
            if (queryId.back() == '*') {
              queryId.pop_back();
            }
            std::string const url(
                "/_db/" +
                arangodb::basics::StringUtils::urlEncode(vocbase->_name) +
                "/_api/aql/shutdown/" + queryId);
            std::map<std::string, std::string> headers;
            auto res =
                cc->syncRequest("", coordTransactionID, "shard:" + shardId,
                                arangodb::rest::HttpRequest::HTTP_REQUEST_PUT,
                                url, "{\"code\": 0}", headers, 120.0);
            // Ignore result, we need to try to remove all.
            // However, log the incident if we have an errorMessage.
            if (res->errorMessage.length() > 0) {
              std::string msg("while trying to unregister query ");
              msg += queryId + std::string("from shard: ") + shardId +
                     std::string("communication failed: ") + res->errorMessage;
              LOG(WARN) << "" << msg.c_str();
            }
          } else {
            // Remove query from registry:
            try {
              queryRegistry->destroy(
                  vocbase, arangodb::basics::StringUtils::uint64(queryId),
                  TRI_ERROR_INTERNAL);
            } catch (...) {
              // Ignore problems
            }
          }
        }
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
    root->initialize();
    root->initializeCursor(nullptr, 0);

    return engine;
  } catch (...) {
    if (!isCoordinator) {
      delete engine;
    }
    throw;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a block to the engine
////////////////////////////////////////////////////////////////////////////////

void ExecutionEngine::addBlock(ExecutionBlock* block) {
  TRI_ASSERT(block != nullptr);

  _blocks.emplace_back(block);
}
