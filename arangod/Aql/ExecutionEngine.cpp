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
    case ExecutionNode::GATHER: {
      return new GatherBlock(engine,
                             static_cast<GatherNode const*>(en));
    }
    case ExecutionNode::REMOTE: {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "unexpected node type 'remote'");
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

ExecutionEngine::ExecutionEngine (AQL_TRANSACTION_V8* trx, 
                                  Query* query)
  : _stats(),
    _blocks(),
    _root(nullptr),
    _trx(trx),
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
    else if (nodeType == ExecutionNode::REMOTE ||
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

  AQL_TRANSACTION_V8*      trx;
  Query*                   query;
  QueryRegistry*           queryRegistry;
  ExecutionBlock*          root;
  EngineLocation           currentLocation;
  size_t                   currentEngineId;
  std::vector<EngineInfo>  engines;
  std::vector<size_t>      engineIds; // stack of engine ids, used for subqueries

  
  CoordinatorInstanciator (AQL_TRANSACTION_V8* trx,
                           Query* query,
                           QueryRegistry* queryRegistry)
    : trx(trx),
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
    
    for (auto it = engines.rbegin(); it != engines.rend(); ++it) {
      if ((*it).location == COORDINATOR) {
        // create a coordinator-based engine
        engine = buildEngineCoordinator((*it));

        TRI_ASSERT(engine != nullptr);

        if ((*it).id > 0) {
          // we need to instanciate this engine in the registry

          // create some fake remote id for the engine that we can pass to
          // the plans to be created on the db-servers
          id = TRI_NewTickServer();
          
          
          // TODO: check if we can register the same query object multiple times
          // or if we need to clone it
          queryRegistry->insert(query->vocbase(), id, query, 3600.0);
        }
      }
      else {
        // create an engine on a remote DB server
        // hand in the previous engine's id
        buildEngineDBServer((*it), id);
      }
    }

    TRI_ASSERT(engine != nullptr);

    // return the last created coordinator-based engine
    // this is the local engine that we'll use to run the query
    return engine;
  }


  void buildEngineDBServer (EngineInfo& info,
                            QueryId connectedId) {
    Collection const* collection = nullptr;

    for (auto en = info.nodes.rbegin(); en != info.nodes.rend(); ++en) {
      if ((*en)->getType() == ExecutionNode::REMOTE) {
        // remove the node's dependencies so it becomes a terminal node
        collection = static_cast<RemoteNode*>((*en))->collection();
        (*en)->removeDependencies();
        // we should only have one remote node
        break;
      }
    }

    TRI_ASSERT(collection != nullptr);
  
    // create a JSON representation of the plan
    triagens::basics::Json result = info.nodes.front()->toJson(TRI_UNKNOWN_MEM_ZONE, true);
    
    // add the collection
    triagens::basics::Json jsonCollectionList(triagens::basics::Json::List);
    triagens::basics::Json json(triagens::basics::Json::Array);
    jsonCollectionList(json("name", triagens::basics::Json(collection->name))
                           ("type", triagens::basics::Json(TRI_TransactionTypeGetStr(collection->accessType))));

    result.set("collections", jsonCollectionList);

    std::string const body(triagens::basics::JsonHelper::toString(result.json()));
    
    std::cout << "GENERATED A PLAN FOR THE REMOTE SERVERS: " << body << "\n";
    
    // now send the plan to the remote servers
    auto cc = triagens::arango::ClusterComm::instance();
    TRI_ASSERT(cc != nullptr);
                             
    std::string const url("/_db/" + triagens::basics::StringUtils::urlEncode(collection->vocbase->_name) + "/_api/aql/instanciate");
    triagens::arango::CoordTransactionID coordTransactionID = TRI_NewTickServer();

    auto&& shardIds = collection->shardIds();
    for (auto shardId : shardIds) {
      // TODO: pass connectedId to the shard so it can fetch data using the correct query id
      auto headers = new std::map<std::string, std::string>;
      auto res = cc->asyncRequest("", 
                                  coordTransactionID,
                                  "shard:" + shardId,  
                                  triagens::rest::HttpRequest::HTTP_REQUEST_POST, 
                                  url,
                                  &body,
                                  false,
                                  headers,
                                  nullptr,
                                  30.0);

      if (res != nullptr) {
        delete res;
      }
    }
  
    int count = 0;
    int nrok = 0;
    for (count = (int) shardIds.size(); count > 0; count--) {
      auto res = cc->wait( "", coordTransactionID, 0, "", 30.0);

      if (res->status == triagens::arango::CL_COMM_RECEIVED) {
        if (res->answer_code == triagens::rest::HttpResponse::OK ||
            res->answer_code == triagens::rest::HttpResponse::CREATED) {
          // query instanciated without problems
          nrok++;
        }
      }
      delete res;
    }

    if (nrok != (int) shardIds.size()) {
      // TODO: provide sensible error message with more details
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "did not receive response from all shards");
    }
  }


  ExecutionEngine* buildEngineCoordinator (EngineInfo& info) {
    std::unique_ptr<ExecutionEngine> engine(new ExecutionEngine(trx, query));

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
          ExecutionBlock* r = new RemoteBlock(engine.get(), remoteNode, "shard:" + shardId, "", "" /*TODO*/);
      
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
    
      if (nodeType == ExecutionNode::RETURN ||
          nodeType == ExecutionNode::REMOVE ||
          nodeType == ExecutionNode::INSERT ||
          nodeType == ExecutionNode::UPDATE ||
          nodeType == ExecutionNode::REPLACE) {
        // set the new root node
        engine->root(eb);
      }
      // TODO: handle subqueries
 
      cache.emplace(std::make_pair((*en), eb));
    }

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

ExecutionEngine* ExecutionEngine::instanciateFromPlan (QueryRegistry* registry,
                                                       AQL_TRANSACTION_V8* trx,
                                                       Query* query,
                                                       ExecutionPlan* plan) {
  ExecutionEngine* engine = nullptr;

  try {
    if (! plan->varUsageComputed()) {
      plan->findVarUsage();
    }

    ExecutionBlock* root = nullptr;

    if (triagens::arango::ServerState::instance()->isCoordinator()) {
      // instanciate the engine on the coordinator
      // TODO: must pass an instance of query registry to the coordinator instanciator!
      std::unique_ptr<CoordinatorInstanciator> inst(new CoordinatorInstanciator(trx, query, nullptr));
      plan->root()->walk(inst.get());

      engine = inst.get()->buildEngines(); 
      root = engine->root();
    }
    else {
      // instanciate the engine on a local server
      engine = new ExecutionEngine(trx, query);
      std::unique_ptr<Instanciator> inst(new Instanciator(engine));
      plan->root()->walk(inst.get());
      root = inst.get()->root;
    }

    root->staticAnalysis();
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
