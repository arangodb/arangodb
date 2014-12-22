////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, execution engine
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/QueryRegistry.h"
#include "Aql/WalkerWorker.h"
#include "Cluster/ClusterComm.h"
#include "Utils/Exception.h"

using namespace triagens::aql;
using namespace triagens::arango;
using Json = triagens::basics::Json;

////////////////////////////////////////////////////////////////////////////////
/// @brief helper function to create a block
////////////////////////////////////////////////////////////////////////////////

static ExecutionBlock* createBlock (ExecutionEngine* engine,
                                    ExecutionNode const* en,
                                    std::unordered_map<ExecutionNode*, ExecutionBlock*> const& cache) {
  switch (en->getType()) {
    case ExecutionNode::SINGLETON: {
      return new SingletonBlock(engine, static_cast<SingletonNode const*>(en));
    }
    case ExecutionNode::INDEX_RANGE: {
      return new IndexRangeBlock(engine, static_cast<IndexRangeNode const*>(en));
    }
    case ExecutionNode::ENUMERATE_COLLECTION: {
      return new EnumerateCollectionBlock(engine,
                                          static_cast<EnumerateCollectionNode const*>(en));
    }
    case ExecutionNode::ENUMERATE_LIST: {
      return new EnumerateListBlock(engine,
                                    static_cast<EnumerateListNode const*>(en));
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
    case ExecutionNode::AGGREGATE: {
      return new AggregateBlock(engine, static_cast<AggregateNode const*>(en));
    }
    case ExecutionNode::SUBQUERY: {
      auto es = static_cast<SubqueryNode const*>(en);
      auto it = cache.find(es->getSubquery());

      TRI_ASSERT(it != cache.end());

      return new SubqueryBlock(engine,
                               static_cast<SubqueryNode const*>(en),
                               it->second);
    }
    case ExecutionNode::RETURN: {
      return new ReturnBlock(engine,
                              static_cast<ReturnNode const*>(en));
    }
    case ExecutionNode::REMOVE: {
      return new RemoveBlock(engine,
                             static_cast<RemoveNode const*>(en));
    }
    case ExecutionNode::INSERT: {
      return new InsertBlock(engine,
                             static_cast<InsertNode const*>(en));
    }
    case ExecutionNode::UPDATE: {
      return new UpdateBlock(engine,
                             static_cast<UpdateNode const*>(en));
    }
    case ExecutionNode::REPLACE: {
      return new ReplaceBlock(engine,
                              static_cast<ReplaceNode const*>(en));
    }
    case ExecutionNode::NORESULTS: {
      return new NoResultsBlock(engine,
                                static_cast<NoResultsNode const*>(en));
    }
    case ExecutionNode::SCATTER: {
      auto&& shardIds = static_cast<ScatterNode const*>(en)->collection()->shardIds();
      return new ScatterBlock(engine,
                              static_cast<ScatterNode const*>(en),
                              shardIds);
    }
    case ExecutionNode::DISTRIBUTE: {
      auto&& shardIds = static_cast<DistributeNode const*>(en)->collection()->shardIds();
      return new DistributeBlock(engine,
                                 static_cast<DistributeNode const*>(en),
                                 shardIds,
                                 static_cast<DistributeNode const*>
                                 (en)->collection());
    }
    case ExecutionNode::GATHER: {
      return new GatherBlock(engine,
                             static_cast<GatherNode const*>(en));
    }
    case ExecutionNode::REMOTE: {
      auto remote = static_cast<RemoteNode const*>(en);
      return new RemoteBlock(engine,
                             remote,
                             remote->server(),
                             remote->ownName(),
                             remote->queryId());
    }
    case ExecutionNode::ILLEGAL: {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "illegal node type");
    }
  }

  return nullptr;
}

// -----------------------------------------------------------------------------
// --SECTION--                                             class ExecutionEngine
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the engine
////////////////////////////////////////////////////////////////////////////////

ExecutionEngine::ExecutionEngine (Query* query)
  : _stats(),
    _blocks(),
    _root(nullptr),
    _query(query),
    _wasShutdown(false),
    _previouslyLockedShards(nullptr),
    _lockedShards(nullptr) {

  _blocks.reserve(8);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the engine, frees all assigned blocks
////////////////////////////////////////////////////////////////////////////////

ExecutionEngine::~ExecutionEngine () {
  try {
    shutdown(TRI_ERROR_INTERNAL);
  }
  catch (...) {
    // shutdown can throw - ignore it in the destructor
  }

  for (auto it = _blocks.begin(); it != _blocks.end(); ++it) {
    delete (*it);
  }
}

////////////////////////////////////////////////////////////////////////////////
// @brief whether or not we are a coordinator
////////////////////////////////////////////////////////////////////////////////
       
bool ExecutionEngine::isCoordinator () {
  return triagens::arango::ServerState::instance()->isCoordinator();
}

////////////////////////////////////////////////////////////////////////////////
// @brief whether or not we are a db server
////////////////////////////////////////////////////////////////////////////////
       
bool ExecutionEngine::isDBServer () {
  return triagens::arango::ServerState::instance()->isDBserver();
}

// -----------------------------------------------------------------------------
// --SECTION--                     walker class for ExecutionNode to instanciate
// -----------------------------------------------------------------------------

struct Instanciator : public WalkerWorker<ExecutionNode> {
  ExecutionEngine* engine;
  ExecutionBlock*  root;
  std::unordered_map<ExecutionNode*, ExecutionBlock*> cache;

  Instanciator (ExecutionEngine* engine) 
    : engine(engine),
      root(nullptr) {
  }
  
  ~Instanciator () {
  }

  virtual void after (ExecutionNode* en) override final {
    ExecutionBlock* eb = createBlock(engine, en, cache);
        
    if (eb == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "illegal node type");
    }

    // do we need to adjust the root node?
    auto const nodeType = en->getType();

    if (nodeType == ExecutionNode::RETURN ||
        nodeType == ExecutionNode::REMOVE ||
        nodeType == ExecutionNode::INSERT ||
        nodeType == ExecutionNode::UPDATE ||
        nodeType == ExecutionNode::REPLACE) {
      root = eb;
    }
    else if (nodeType == ExecutionNode::DISTRIBUTE ||
             nodeType == ExecutionNode::SCATTER ||
             nodeType == ExecutionNode::GATHER) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "logic error, got cluster node in local query");
    }

    try {
      engine->addBlock(eb);
    }
    catch (...) {
      delete eb;
      throw;
    }

    // Now add dependencies:
    std::vector<ExecutionNode*> deps = en->getDependencies();
    for (auto it = deps.begin(); it != deps.end(); ++it) {
      auto it2 = cache.find(*it);
      TRI_ASSERT(it2 != cache.end());
      eb->addDependency(it2->second);
    }

    if (root == nullptr &&
        en->getParents().empty()) {
      // adjust the root node if none was set already
      root = eb;
    }

    cache.emplace(std::make_pair(en, eb));
  }

};

// -----------------------------------------------------------------------------
// --SECTION--                     walker class for ExecutionNode to instanciate
// -----------------------------------------------------------------------------

// Here is a description of how the instanciation of an execution plan
// works in the cluster. See below for a complete example
//
// The instanciation of this works as follows:
// (0) Variable usage and register planning is done in the global plan
// (1) A walk with subqueries is done on the whole plan
//     The purpose is to plan how many ExecutionEngines we need, where they
//     have to be instanciated and which plan nodes belong to each of them.
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
//     of that node) are always already instanciated before the RemoteNode
//     is instanciated. The corresponding query ids are collected in a
//     global hash table, for which the key consists of the id of the 
//     RemoteNode using the query and the actual query id. For each engine,
//     the nodes are instanciated along the list of nodes for that engine.
//     This means that all dependencies of a node N are already instanciated
//     when N is instanciated. We distintuish the coordinator and the
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
// a proper instanciation of the whole thing.


struct CoordinatorInstanciator : public WalkerWorker<ExecutionNode> {
  enum EngineLocation {
    COORDINATOR,
    DBSERVER
  };

  struct EngineInfo {
    EngineInfo (EngineLocation location,
                size_t id,
                triagens::aql::QueryPart p,
                size_t idOfRemoteNode)
      : location(location),
        id(id),
        nodes(),
        part(p), 
        idOfRemoteNode(idOfRemoteNode) {
    }

    Collection* getCollection () const {
      Collection* collection = nullptr;
     
      for (auto en = nodes.rbegin(); en != nodes.rend(); ++en) {
        // find the collection to be used 
        if ((*en)->getType() == ExecutionNode::ENUMERATE_COLLECTION) {
          collection = const_cast<Collection*>(static_cast<EnumerateCollectionNode*>((*en))->collection());
        }
        else if ((*en)->getType() == ExecutionNode::INDEX_RANGE) {
          collection = const_cast<Collection*>(static_cast<IndexRangeNode*>((*en))->collection());
        }
        else if ((*en)->getType() == ExecutionNode::INSERT ||
                 (*en)->getType() == ExecutionNode::UPDATE ||
                 (*en)->getType() == ExecutionNode::REPLACE ||
                 (*en)->getType() == ExecutionNode::REMOVE) {
          collection = const_cast<Collection*>(static_cast<ModificationNode*>((*en))->collection());
        }
      }

      TRI_ASSERT(collection != nullptr);
      return collection;
    }

    EngineLocation const         location;
    size_t const                 id;
    std::vector<ExecutionNode*>  nodes;
    triagens::aql::QueryPart     part;   // only relevant for DBserver parts
    size_t                       idOfRemoteNode;  // id of the remote node
                // in the original plan that needs this engine
  };

  Query*                   query;
  QueryRegistry*           queryRegistry;
  ExecutionBlock*          root;
  EngineLocation           currentLocation;
  size_t                   currentEngineId;
  std::vector<EngineInfo>  engines;
  std::vector<size_t>      engineStack;  // stack of engine ids, used for
                                         // RemoteNodes
  std::unordered_set<std::string> collNamesSeenOnDBServer;  
     // names of sharded collections that we have already seen on a DBserver
     // this is relevant to decide whether or not the engine there is a main
     // query or a dependent one.
  std::unordered_map<std::string, std::string> queryIds;
     // this map allows to find the queries which are the parts of the big
     // query. There are two cases, the first is for the remote queries on
     // the DBservers, for these, the key is:
     //   itoa(ID of RemoteNode in original plan) + "_" + shardId
     // and the value is the
     //   queryId on DBserver
     // with a * appended, if it is a PART_MAIN query.
     // The second case is a query, which lives on the coordinator but is not
     // the main query. For these, we store
     //   itoa(ID of RemoteNode in original plan)
     // and the value is the
     //   queryId used in the QueryRegistry
     // this is built up when we instanciate the various engines on the
     // DBservers and used when we instanciate the ones on the
     // coordinator. Note that the main query and engine is not put into
     // this map at all.

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

  CoordinatorInstanciator (Query* query,
                           QueryRegistry* queryRegistry)
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
 
////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

  ~CoordinatorInstanciator () {
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief generatePlanForOneShard
////////////////////////////////////////////////////////////////////////////////

  triagens::basics::Json generatePlanForOneShard (EngineInfo const& info,
                                                  QueryId& connectedId,
                                                  std::string const& shardId,
                                                  bool verbose) {
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
        static_cast<RemoteNode*>(clone)->server("server:" + triagens::arango::ServerState::instance()->getId());
        static_cast<RemoteNode*>(clone)->ownName(shardId);
        static_cast<RemoteNode*>(clone)->queryId(connectedId);
      }
    
      if (previous != nullptr) {
        clone->addDependency(previous);
      }

      previous = clone;
    }
    plan.root(previous);
    plan.setVarUsageComputed();
    return plan.root()->toJson(TRI_UNKNOWN_MEM_ZONE, verbose);
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief distributePlanToShard, send a single plan to one shard
////////////////////////////////////////////////////////////////////////////////

  void distributePlanToShard (triagens::arango::CoordTransactionID& coordTransactionID,
                              EngineInfo const& info,
                              Collection* collection,
                              QueryId& connectedId,
                              std::string const& shardId, 
                              TRI_json_t* jsonPlan) {
    // create a JSON representation of the plan
    Json result(Json::Array);

    // inject the current shard id into the collection
    collection->setCurrentShard(shardId);

    Json jsonNodesList(TRI_UNKNOWN_MEM_ZONE, jsonPlan, Json::NOFREE);
    
    // add the collection
    Json jsonCollectionsList(Json::List);
    Json json(Json::Array);
    jsonCollectionsList(json("name", Json(collection->getName()))
                            ("type", Json(TRI_TransactionTypeGetStr(collection->accessType))));

    jsonNodesList.set("collections", jsonCollectionsList);
    jsonNodesList.set("variables", query->ast()->variables()->toJson(TRI_UNKNOWN_MEM_ZONE));

    result.set("plan", jsonNodesList);
    if (info.part == triagens::aql::PART_MAIN) {
      result.set("part", Json("main"));
    }
    else {
      result.set("part", Json("dependent"));
    }

    Json optimizerOptionsRules(Json::List);
    Json optimizerOptions(Json::Array);

    Json options(Json::Array);
    optimizerOptionsRules.add(Json("-all"));
    optimizerOptions.set("rules", optimizerOptionsRules);
    options.set("optimizer", optimizerOptions);
    result.set("options", options);
    std::unique_ptr<std::string> body(new std::string(triagens::basics::JsonHelper::toString(result.json())));
    
    // std::cout << "GENERATED A PLAN FOR THE REMOTE SERVERS: " << *(body.get()) << "\n";
    
    auto cc = triagens::arango::ClusterComm::instance();

    std::string const url("/_db/" + triagens::basics::StringUtils::urlEncode(collection->vocbase->_name) + 
                          "/_api/aql/instanciate");

    auto headers = new std::map<std::string, std::string>;
    (*headers)["X-Arango-Nolock"] = shardId;   // Prevent locking
    auto res = cc->asyncRequest("", 
                                coordTransactionID,
                                "shard:" + shardId,  
                                triagens::rest::HttpRequest::HTTP_REQUEST_POST, 
                                url,
                                body.release(),
                                true,
                                headers,
                                nullptr,
                                30.0);

    if (res != nullptr) {
      delete res;
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief aggregateQueryIds, get answers for all shards in a Scatter/Gather
////////////////////////////////////////////////////////////////////////////////

  void aggregateQueryIds (EngineInfo const& info,
                     triagens::arango::ClusterComm*& cc,
                     triagens::arango::CoordTransactionID& coordTransactionID,
                     Collection* collection) {

    // pick up the remote query ids
    std::vector<std::string> shardIds = collection->shardIds();

    std::string error;
    int count = 0;
    int nrok = 0;
    for (count = (int) shardIds.size(); count > 0; count--) {
      auto res = cc->wait("", coordTransactionID, 0, "", 30.0);

      if (res->status == triagens::arango::CL_COMM_RECEIVED) {
        if (res->answer_code == triagens::rest::HttpResponse::OK ||
            res->answer_code == triagens::rest::HttpResponse::CREATED ||
            res->answer_code == triagens::rest::HttpResponse::ACCEPTED) {
          // query instanciated without problems
          nrok++;

          // pick up query id from response
          triagens::basics::Json response(TRI_UNKNOWN_MEM_ZONE, triagens::basics::JsonHelper::fromString(res->answer->body()));
          std::string queryId = triagens::basics::JsonHelper::getStringValue(response.json(), "queryId", "");

          // std::cout << "DB SERVER ANSWERED WITHOUT ERROR: " << res->answer->body() << ", REMOTENODEID: " << info.idOfRemoteNode << " SHARDID:"  << res->shardID << ", QUERYID: " << queryId << "\n";
          std::string theID
            = triagens::basics::StringUtils::itoa(info.idOfRemoteNode)
            + "_" + res->shardID;
          if (info.part == triagens::aql::PART_MAIN) {
            queryIds.emplace(theID, queryId+"*");
          }
          else {
            queryIds.emplace(theID, queryId);
          }
        }
        else {
          error += "DB SERVER ANSWERED WITH ERROR: ";
          error += res->answer->body();
          error += "\n";
        }
      }
      else {
        error += std::string("Communication with shard '") + 
          std::string(res->shardID) + 
          std::string("' on cluster node '") +
          std::string(res->serverID) +
          std::string("' failed.");
      }
      delete res;
    }

    // std::cout << "GOT ALL RESPONSES FROM DB SERVERS: " << nrok << "\n";

    if (nrok != (int) shardIds.size()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, error);
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief distributePlansToShards, for a single Scatter/Gather block
////////////////////////////////////////////////////////////////////////////////

  void distributePlansToShards (EngineInfo const& info,
                                QueryId connectedId) {

    // std::cout << "distributePlansToShards: " << info.id << std::endl;
    Collection* collection = info.getCollection();
    // now send the plan to the remote servers
    triagens::arango::CoordTransactionID coordTransactionID = TRI_NewTickServer();
    auto cc = triagens::arango::ClusterComm::instance();
    TRI_ASSERT(cc != nullptr);

    // iterate over all shards of the collection
    for (auto & shardId : collection->shardIds()) {
      // inject the current shard id into the collection
      collection->setCurrentShard(shardId);
      auto jsonPlan = generatePlanForOneShard(info, connectedId, shardId, true);

      distributePlanToShard(coordTransactionID, info, collection, connectedId, shardId, jsonPlan.steal());
    }

    // fix collection  
    collection->resetCurrentShard();
    aggregateQueryIds(info, cc, coordTransactionID, collection);
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief buildEngineCoordinator, for a single piece
////////////////////////////////////////////////////////////////////////////////

  ExecutionEngine* buildEngineCoordinator (EngineInfo& info) {
    Query* localQuery = query;
    bool needToClone = info.id > 0;   // use the original for the main part
    if (needToClone) {
      // need a new query instance on the coordinator
      localQuery = query->clone(PART_DEPENDENT, false);
      if (localQuery == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,"cannot clone query");
      }
    }

    try {
      std::unique_ptr<ExecutionEngine> engine(new ExecutionEngine(localQuery));
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
        ExecutionBlock* eb = createBlock(engine.get(), (*en), cache);
         
        if (eb == nullptr) {
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "illegal node type");
        }
      
        try {
          engine.get()->addBlock(eb);
        }
        catch (...) {
          delete eb;
          throw;
        }
        
        std::vector<ExecutionNode*> deps = (*en)->getDependencies();

        for (auto dep = deps.begin(); dep != deps.end(); ++dep) {
          auto d = cache.find(*dep);

          if (d != cache.end()) {
            // add regular dependencies
            eb->addDependency((*d).second);
          }
        }

        if (nodeType == ExecutionNode::GATHER) {
          // we found a gather node
          TRI_ASSERT(remoteNode != nullptr);

          // now we'll create a remote node for each shard and add it to the gather node
          Collection const* collection = nullptr;
          if (nodeType == ExecutionNode::GATHER) {
            collection = static_cast<GatherNode const*>((*en))->collection();
          }
          else {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
          }
          
          auto&& shardIds = collection->shardIds();

          for (auto const& shardId : shardIds) {
            std::string theId 
              = triagens::basics::StringUtils::itoa(remoteNode->id())
              + "_" + shardId;
            auto it = queryIds.find(theId);
            if (it == queryIds.end()) {
              THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "could not find query id in list");
            }
            std::string idThere = it->second;
            if (idThere.back() == '*') {
              idThere.pop_back();
            }
            ExecutionBlock* r = new RemoteBlock(engine.get(), 
                                                remoteNode, 
                                                "shard:" + shardId, // server
                                                "",                 // ownName
                                                idThere);           // queryId
        
            try {
              engine.get()->addBlock(r);
            }
            catch (...) {
              delete r;
              throw;
            }

            eb->addDependency(r);
          }
        }
        
        // the last block is always the root 
        engine->root(eb);
   
        // put it into our cache:
        cache.emplace(std::make_pair((*en), eb));
      }

      TRI_ASSERT(engine->root() != nullptr);

      // localQuery is stored in the engine
      return engine.release();
    }
    catch (...) {
      localQuery->engine(nullptr);  // engine is already destroyed by unique_ptr
      if (needToClone) {
        delete localQuery;
      }
      throw;
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief buildEngines, build engines on DBservers and coordinator
////////////////////////////////////////////////////////////////////////////////

  ExecutionEngine* buildEngines () {
    ExecutionEngine* engine = nullptr;
    QueryId id              = 0;
    
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
          }
          catch (...) {
            delete engine->getQuery();
            // This deletes the new query as well as the engine
            throw;
          }
          try {
            std::string queryId = triagens::basics::StringUtils::itoa(id);
            std::string theID
              = triagens::basics::StringUtils::itoa(it->idOfRemoteNode)
              + "/" + engine->getQuery()->vocbase()->_name;
            queryIds.emplace(theID, queryId);
          }
          catch (...) {
            queryRegistry->destroy(engine->getQuery()->vocbase(),
                                   id, TRI_ERROR_INTERNAL);
            // This deletes query, engine and entry in QueryRegistry
            throw;
          }
        }
      }
      else {
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
 
////////////////////////////////////////////////////////////////////////////////
/// @brief before method for collection of pieces phase
////////////////////////////////////////////////////////////////////////////////

  virtual bool before (ExecutionNode* en) override final {
    auto const nodeType = en->getType();

    if (nodeType == ExecutionNode::REMOTE) {
      // got a remote node
      // this indicates the end of an execution section

      engineStack.push_back(currentEngineId);

      // begin a new engine
      // flip current location
      currentLocation = (currentLocation == COORDINATOR ? DBSERVER : COORDINATOR);
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

////////////////////////////////////////////////////////////////////////////////
/// @brief after method for collection of pieces phase
////////////////////////////////////////////////////////////////////////////////

  virtual void after (ExecutionNode* en) override final {
    auto const nodeType = en->getType();

    if (nodeType == ExecutionNode::REMOTE) {
      currentEngineId = engineStack.back();
      engineStack.pop_back();
      currentLocation = engines[currentEngineId].location;
    }

    // assign the current node to the current engine
    engines[currentEngineId].nodes.push_back(en);
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create an execution engine from a plan
////////////////////////////////////////////////////////////////////////////////

ExecutionEngine* ExecutionEngine::instanciateFromPlan (QueryRegistry* queryRegistry,
                                                       Query* query,
                                                       ExecutionPlan* plan,
                                                       bool planRegisters) {
  ExecutionEngine* engine = nullptr;

  try {
    if (! plan->varUsageComputed()) {
      plan->findVarUsage();
    }
    if (planRegisters) {
      plan->planRegisters();
    }

    ExecutionBlock* root = nullptr;

    if (isCoordinator()) {
      // instanciate the engine on the coordinator
      std::unique_ptr<CoordinatorInstanciator> inst(new CoordinatorInstanciator(query, queryRegistry));
      plan->root()->walk(inst.get());

      // std::cout << "ORIGINAL PLAN:\n" << plan->toJson(query->ast(), TRI_UNKNOWN_MEM_ZONE, true).toString() << "\n\n";

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
          engine->_lockedShards = new std::unordered_set<std::string>(*Transaction::_makeNolockHeaders);
          engine->_previouslyLockedShards = Transaction::_makeNolockHeaders;
        }
        else {
          engine->_lockedShards = new std::unordered_set<std::string>();
          engine->_previouslyLockedShards = nullptr;
        }
        std::map<std::string, std::string> forLocking;
        for (auto& q : inst.get()->queryIds) {
          std::string theId = q.first;
          std::string queryId = q.second;
          auto pos = theId.find('_');
          if (pos != std::string::npos) {
            // So this is a remote one on a DBserver:
            if (queryId.back() == '*') {  // only the PART_MAIN one!
              queryId.pop_back();
              std::string shardId = theId.substr(pos+1);
              engine->_lockedShards->insert(shardId);
              forLocking.emplace(shardId, queryId);
            }
          }
        }
        // Now lock them all in the right order:
        for (auto& p : forLocking) {
          std::string const& shardId = p.first;
          std::string const& queryId = p.second;
          // Lock shard on DBserver:
          triagens::arango::CoordTransactionID coordTransactionID 
              = TRI_NewTickServer();
          auto cc = triagens::arango::ClusterComm::instance();
          TRI_vocbase_t* vocbase = query->vocbase();
          std::string const url("/_db/"
                   + triagens::basics::StringUtils::urlEncode(vocbase->_name)
                   + "/_api/aql/lock/" + queryId);
          std::map<std::string, std::string> headers;
          auto res = cc->syncRequest("", coordTransactionID,
             "shard:" + shardId,  
             triagens::rest::HttpRequest::HTTP_REQUEST_PUT, url, "{}", 
             headers, 30.0);
          if (res->status != CL_COMM_SENT) {
            delete res;
            THROW_ARANGO_EXCEPTION_MESSAGE(
                TRI_ERROR_QUERY_COLLECTION_LOCK_FAILED,
                "could not lock all shards");
          }
          delete res;
        }
        Transaction::_makeNolockHeaders = engine->_lockedShards;
      }
      catch (...) {
        // We need to destroy all queries that we have built and stuffed
        // into the QueryRegistry as well as those that we have pushed to
        // the DBservers via HTTP:
        TRI_vocbase_t* vocbase = query->vocbase();
        for (auto& q : inst.get()->queryIds) {
          std::string theId = q.first;
          std::string queryId = q.second;
          auto pos = theId.find('_');
          if (pos != std::string::npos) {
            // So this is a remote one on a DBserver:
            std::string shardId = theId.substr(pos+1);
            // Remove query from DBserver:
            triagens::arango::CoordTransactionID coordTransactionID 
                = TRI_NewTickServer();
            auto cc = triagens::arango::ClusterComm::instance();
            if (queryId.back() == '*') {
              queryId.pop_back();
            }
            std::string const url("/_db/"
                     + triagens::basics::StringUtils::urlEncode(vocbase->_name)
                     + "/_api/aql/shutdown/" + queryId);
            std::map<std::string, std::string> headers;
            auto res = cc->syncRequest("", coordTransactionID,
               "shard:" + shardId,  
               triagens::rest::HttpRequest::HTTP_REQUEST_PUT, url, 
                 "{\"code\": 0}", headers, 30.0);
            // Ignore result
            delete res;
          }
          else {
            // Remove query from registry:
            try {
              queryRegistry->destroy(vocbase,
                             triagens::basics::StringUtils::uint64(queryId),
                             TRI_ERROR_INTERNAL);
            }
            catch (...) {
              // Ignore problems
            }
          }
        }
        throw;
      }
    }
    else {
      // instanciate the engine on a local server
      engine = new ExecutionEngine(query);
      std::unique_ptr<Instanciator> inst(new Instanciator(engine));
      plan->root()->walk(inst.get());
      root = inst.get()->root;
    }

    TRI_ASSERT(root != nullptr);
    engine->_root = root;
    root->initialize();
    root->initializeCursor(nullptr, 0);
  
    return engine;
  }
  catch (...) {
    if (! isCoordinator()) {
      delete engine;
    }
    throw;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a block to the engine
////////////////////////////////////////////////////////////////////////////////

void ExecutionEngine::addBlock (ExecutionBlock* block) {
  TRI_ASSERT(block != nullptr);

  _blocks.push_back(block);
}


// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
