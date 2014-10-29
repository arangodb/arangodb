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
    _wasShutdown(false) {

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

  std::vector<std::pair<std::string, TRI_json_t*>> _plans;

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
     // map from itoa(ID of RemoteNode in original plan) + shardId
     //     to   queryId on DBserver
     // this is built up when we instanciate the various engines on the
     // DBservers and used when we instanciate the one using them on the
     // coordinator.

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
 
  void resetPlans() {
    _plans.clear();
  }
  ~CoordinatorInstanciator () {
    resetPlans();
  }

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
      auto clone = current->clone(&plan, false, true);
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

  void generatePlansForDBServers (EngineInfo const& info,
                                  QueryId connectedId,
                                  bool verbose) {
    Collection* collection = info.getCollection();
    
    // iterate over all shards of the collection
    for (auto & shardId : collection->shardIds()) {
      // inject the current shard id into the collection
      collection->setCurrentShard(shardId);
      auto jsonPlan = generatePlanForOneShard(info, connectedId, shardId, verbose);
      _plans.push_back(std::make_pair(shardId, jsonPlan.steal()));  
    }
    // fix collection  
    collection->resetCurrentShard();
  }


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

          queryRegistry->insert(id, engine->getQuery(), 3600.0);
        }
      }
      else {
        // create an engine on a remote DB server
        // hand in the previous engine's id
        generatePlansForDBServers((*it), id, true);
        distributePlansToShards((*it), id);
        resetPlans();
      }
    }

    TRI_ASSERT(engine != nullptr);

    // return the last created coordinator-based engine
    // this is the local engine that we'll use to run the query
    return engine;
  }
 
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
    
    // TODO: pass connectedId to the shard so it can fetch data using the correct query id
    auto cc = triagens::arango::ClusterComm::instance();

    std::string const url("/_db/" + triagens::basics::StringUtils::urlEncode(collection->vocbase->_name) + 
                          "/_api/aql/instanciate");

    auto headers = new std::map<std::string, std::string>;
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
            + res->shardID;
          queryIds.emplace(std::make_pair(theID, queryId));
        }
        else {
          // std::cout << "DB SERVER ANSWERED WITH ERROR: " << res->answer->body() << "\n";
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
      // TODO: provide sensible error message with more details
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, error);
    }
  }

  void distributePlansToShards (
             EngineInfo const& info,
             QueryId connectedId) {

    // std::cout << "distributePlansToShards: " << info.id << std::endl;
    Collection* collection = info.getCollection();
    // now send the plan to the remote servers
    triagens::arango::CoordTransactionID coordTransactionID = TRI_NewTickServer();
    auto cc = triagens::arango::ClusterComm::instance();
    TRI_ASSERT(cc != nullptr);

    // iterate over all shards of the collection
    for (auto onePlan: _plans) {
      collection->setCurrentShard(onePlan.first);

      distributePlanToShard(coordTransactionID, info, collection, connectedId, onePlan.first, onePlan.second);
      onePlan.second = nullptr;
    }

    // fix collection  
    collection->resetCurrentShard();
    aggregateQueryIds(info, cc, coordTransactionID, collection);
  }


  ExecutionEngine* buildEngineCoordinator (EngineInfo& info) {
    // need a new query instance on the coordinator
    auto clone = query->clone(PART_DEPENDENT, false);

    std::unique_ptr<ExecutionEngine> engine(new ExecutionEngine(clone));
    clone->engine(engine.get());

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
            + shardId;
          auto it = queryIds.find(theId);
          if (it == queryIds.end()) {
            THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "could not find query id in list");
          }

          ExecutionBlock* r = new RemoteBlock(engine.get(), 
                                              remoteNode, 
                                              "shard:" + shardId, // server
                                              "",                 // ownName
                                              (*it).second);      // queryId
      
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

    return engine.release();
  }

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
/// @brief add a block to the engine
////////////////////////////////////////////////////////////////////////////////

void ExecutionEngine::addBlock (ExecutionBlock* block) {
  TRI_ASSERT(block != nullptr);

  _blocks.push_back(block);
}

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

      engine = inst.get()->buildEngines(); 
      root = engine->root();
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
    delete engine;
    throw;
  }
}


// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
