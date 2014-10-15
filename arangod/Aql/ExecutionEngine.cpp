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
    _query(query) {

  _blocks.reserve(8);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the engine, frees all assigned blocks
////////////////////////////////////////////////////////////////////////////////

ExecutionEngine::~ExecutionEngine () {
  if (_root != nullptr) {
    _root->shutdown();
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

  virtual void after (ExecutionNode* en) override {
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
    else if (nodeType == ExecutionNode::SCATTER ||
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
                size_t id)
      : location(location),
        id(id),
        nodes() {
    }

    EngineLocation const         location;
    size_t const                 id;
    std::vector<ExecutionNode*>  nodes;
  };

  std::shared_ptr<AQL_TRANSACTION_V8> trx;
  Query*                   query;
  QueryRegistry*           queryRegistry;
  ExecutionBlock*          root;
  EngineLocation           currentLocation;
  size_t                   currentEngineId;
  std::vector<EngineInfo>  engines;
  std::vector<size_t>      engineIds; // stack of engine ids, used for subqueries

  virtual bool EnterSubQueryFirst () {
    return true;
  }
  
  CoordinatorInstanciator (Query* query,
                           QueryRegistry* queryRegistry)
    : trx(query->getTrxPtr()),
      query(query),
      queryRegistry(queryRegistry),
      root(nullptr),
      currentLocation(COORDINATOR),
      currentEngineId(0),
      engines() {

    TRI_ASSERT(trx != nullptr);
    TRI_ASSERT(query != nullptr);
    TRI_ASSERT(queryRegistry != nullptr);

    engines.emplace_back(EngineInfo(COORDINATOR, 0));
  }
  
  ~CoordinatorInstanciator () {
  }


  ExecutionEngine* buildEngines () {
    ExecutionEngine* engine = nullptr;
    QueryId id              = 0;
    std::unordered_map<std::string, std::string> queryIds;
    
    for (auto it = engines.rbegin(); it != engines.rend(); ++it) {
      if ((*it).location == COORDINATOR) {
        // create a coordinator-based engine
        engine = buildEngineCoordinator((*it), queryIds);

        TRI_ASSERT(engine != nullptr);

        if ((*it).id > 0) {
          Query* otherQuery = query->clone(PART_DEPENDENT);
          otherQuery->engine(engine);

          auto* newPlan = new ExecutionPlan(otherQuery->ast());
          otherQuery->setPlan(newPlan);
    
          // clone all variables 
          for (auto it2 : query->ast()->variables()->variables(true)) {
            auto var = query->ast()->variables()->getVariable(it2.first);
            TRI_ASSERT(var != nullptr);
            otherQuery->ast()->variables()->createVariable(var);
          }

          ExecutionNode const* current = (*it).nodes.front();
          ExecutionNode* previous = nullptr;

          // clone nodes until we reach a remote node
          while (current != nullptr) {
            bool stop = false;

            auto clone = current->clone(newPlan, false, true);
            newPlan->registerNode(clone);
        
            if (previous == nullptr) {
              // set the root node
              newPlan->root(clone);
            }
            else {
              previous->addDependency(clone);
            }

            auto const& deps = current->getDependencies();
            if (deps.size() != 1) {
              stop = true;
            }

            if (stop) {
              break;
            }

            previous = clone;
            current = deps[0];
          }

          // TODO: test if this is necessary or does harm
          // newPlan->setVarUsageComputed();
      
          // we need to instanciate this engine in the registry

          // create a remote id for the engine that we can pass to
          // the plans to be created for the DBServers
          id = TRI_NewTickServer();

          queryRegistry->insert(otherQuery->vocbase(), id, otherQuery, 3600.0);
        }
      }
      else {
        // create an engine on a remote DB server
        // hand in the previous engine's id
        queryIds = buildEngineDBServer((*it), id);
      }
    }

    TRI_ASSERT(engine != nullptr);

    // return the last created coordinator-based engine
    // this is the local engine that we'll use to run the query
    return engine;
  }


  std::unordered_map<std::string, std::string> buildEngineDBServer (EngineInfo const& info,
                                                                    QueryId connectedId) {
    Collection* collection = nullptr;
     
    for (auto en = info.nodes.rbegin(); en != info.nodes.rend(); ++en) {
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
    

    // now send the plan to the remote servers
    auto cc = triagens::arango::ClusterComm::instance();
    TRI_ASSERT(cc != nullptr);
                             
    triagens::arango::CoordTransactionID coordTransactionID = TRI_NewTickServer();
    std::string const url("/_db/" + triagens::basics::StringUtils::urlEncode(collection->vocbase->_name) + 
                          "/_api/aql/instanciate");

    auto&& shardIds = collection->shardIds();
 
    // iterate over all shards of the collection
    for (auto shardId : shardIds) {
      // copy the relevant fragment of the plan for each shard
      ExecutionPlan plan(query->ast());

      ExecutionNode const* current = info.nodes.front();
      ExecutionNode* previous = nullptr;

      // clone nodes until we reach a remote node
      while (current != nullptr) {
        bool stop = false;

        auto clone = current->clone(&plan, false, true);
        plan.registerNode(clone);
        
        if (current->getType() == ExecutionNode::REMOTE) {
          // we'll stop after a remote
          stop = true;

          // update the remote node with the information about the query
          static_cast<RemoteNode*>(clone)->server("server:" + triagens::arango::ServerState::instance()->getId());
          static_cast<RemoteNode*>(clone)->ownName(shardId);
          static_cast<RemoteNode*>(clone)->queryId(connectedId);
        }
     
      
        if (previous == nullptr) {
          // set the root node
          plan.root(clone);
        }
        else {
          previous->addDependency(clone);
        }

        auto const& deps = current->getDependencies();
        if (deps.size() != 1) {
          stop = true;
        }

        if (stop) {
          break;
        }

        previous = clone;
        current = deps[0];
      }
      
      // inject the current shard id into the collection
      collection->setCurrentShard(shardId);
      plan.setVarUsageComputed();

      // create a JSON representation of the plan
      Json result(Json::Array);
      Json jsonNodesList(plan.root()->toJson(TRI_UNKNOWN_MEM_ZONE, true));
    
      // add the collection
      Json jsonCollectionsList(Json::List);
      Json json(Json::Array);
      jsonCollectionsList(json("name", Json(collection->getName()))
                              ("type", Json(TRI_TransactionTypeGetStr(collection->accessType))));

      jsonNodesList.set("collections", jsonCollectionsList);
      jsonNodesList.set("variables", query->ast()->variables()->toJson(TRI_UNKNOWN_MEM_ZONE));

      result.set("plan", jsonNodesList);
      result.set("part", Json("main")); // TODO: set correct query type

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
      
    // fix collection  
    collection->resetCurrentShard();

    // pick up the remote query ids
    std::unordered_map<std::string, std::string> queryIds;
  
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

          // std::cout << "DB SERVER ANSWERED WITHOUT ERROR: " << res->answer->body() << ", SHARDID:"  << res->shardID << ", QUERYID: " << queryId << "\n";
          queryIds.emplace(std::make_pair(res->shardID, queryId));
          
        }
        else {
          // std::cout << "DB SERVER ANSWERED WITH ERROR: " << res->answer->body() << "\n";
        }
      }
      delete res;
    }

    // std::cout << "GOT ALL RESPONSES FROM DB SERVERS: " << nrok << "\n";

    if (nrok != (int) shardIds.size()) {
      // TODO: provide sensible error message with more details
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "did not receive response from all shards");
    }
          
    return queryIds;
  }


  ExecutionEngine* buildEngineCoordinator (EngineInfo& info,
                                           std::unordered_map<std::string, std::string> const& queryIds) {
    std::unique_ptr<ExecutionEngine> engine(new ExecutionEngine(query));

    std::unordered_map<ExecutionNode*, ExecutionBlock*> cache;
    RemoteNode* remoteNode = nullptr;

    for (auto en = info.nodes.rbegin(); en != info.nodes.rend(); ++en) {
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
        auto&& shardIds = static_cast<GatherNode const*>((*en))->collection()->shardIds();

        for (auto shardId : shardIds) {
          // TODO: pass actual queryId into RemoteBlock
          auto it = queryIds.find(shardId);
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
      // TODO: handle subqueries
 
      cache.emplace(std::make_pair((*en), eb));
    }

    TRI_ASSERT(engine->root() != nullptr);

    return engine.release();
  }

  virtual bool enterSubquery (ExecutionNode*, ExecutionNode*) override {
    engineIds.push_back(currentEngineId);
    return true;
  }
  
  virtual void leaveSubquery (ExecutionNode*, ExecutionNode*) override {
    currentEngineId = engineIds.back();
    engineIds.pop_back();
  }
  
  virtual bool before (ExecutionNode* en) override {
    // assign the current node to the current engine
    engines[currentEngineId].nodes.push_back(en);

    auto const nodeType = en->getType();

    if (nodeType == ExecutionNode::REMOTE) {
      // got a remote node
      // this indicates the end of an execution section

      // begin a new engine
      // flip current location
      currentLocation = (currentLocation == COORDINATOR ? DBSERVER : COORDINATOR);
      currentEngineId = engines.size();
      engines.emplace_back(EngineInfo(currentLocation, currentEngineId));
    }

    return false;
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
    root->initialize();
    root->initializeCursor(nullptr, 0);

    engine->_root = root;
  
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
