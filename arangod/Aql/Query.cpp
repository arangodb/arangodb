////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, query context
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

#include "Aql/Query.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/Executor.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Optimizer.h"
#include "Aql/Parser.h"
#include "Aql/QueryCache.h"
#include "Aql/QueryList.h"
#include "Aql/ShortStringStorage.h"
#include "Basics/fasthash.h"
#include "Basics/JsonHelper.h"
#include "Basics/json.h"
#include "Basics/tri-strings.h"
#include "Basics/Exceptions.h"
#include "Cluster/ServerState.h"
#include "Utils/AqlTransaction.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/StandaloneTransactionContext.h"
#include "Utils/V8TransactionContext.h"
#include "V8/v8-conv.h"
#include "V8Server/ApplicationV8.h"
#include "V8Server/v8-shape-conv.h"
#include "VocBase/vocbase.h"
#include "VocBase/Graphs.h"

using namespace triagens::aql;
using Json = triagens::basics::Json;

// -----------------------------------------------------------------------------
// --SECTION--                                               static const values
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief empty string singleton
////////////////////////////////////////////////////////////////////////////////
    
static char const* EmptyString = "";

////////////////////////////////////////////////////////////////////////////////
/// @brief names of query phases / states
////////////////////////////////////////////////////////////////////////////////

static std::string StateNames[] = {
  "initializing",           // INITIALIZATION 
  "parsing",                // PARSING
  "optimizing ast",         // AST_OPTIMIZATION
  "instantiating plan",     // PLAN_INSTANTIATION
  "optimizing plan",        // PLAN_OPTIMIZATION
  "executing",              // EXECUTION
  "finalizing"              // FINALIZATION
};

// make sure the state strings and the actual states match
static_assert(sizeof(StateNames) / sizeof(std::string) == static_cast<size_t>(ExecutionState::INVALID_STATE), 
              "invalid number of ExecutionState values");

// -----------------------------------------------------------------------------
// --SECTION--                                                    struct Profile
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create a profile
////////////////////////////////////////////////////////////////////////////////
      
Profile::Profile (Query* query) 
  : query(query),
    results(),
    stamp(TRI_microtime()),
    tracked(false) {

  auto queryList = static_cast<QueryList*>(query->vocbase()->_queries);

  if (queryList != nullptr) {
    try {
      tracked = queryList->insert(query, stamp);
    }
    catch (...) {
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a profile
////////////////////////////////////////////////////////////////////////////////
      
Profile::~Profile () { 
  // only remove from list when the query was inserted into it...
  if (tracked) {
    auto queryList = static_cast<QueryList*>(query->vocbase()->_queries);

    if (queryList != nullptr) {
      try {
        queryList->remove(query, stamp);
      }
      catch (...) {
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets a state to done
////////////////////////////////////////////////////////////////////////////////

void Profile::setDone (ExecutionState state) {
  double const now = TRI_microtime();

  if (state != ExecutionState::INVALID_STATE) {
    // record duration of state
    results.emplace_back(state, now - stamp); 
  }

  // set timestamp
  stamp = now;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert the profile to JSON
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* Profile::toJson (TRI_memory_zone_t*) {
  triagens::basics::Json result(triagens::basics::Json::Object);
  for (auto const& it : results) {
    result.set(StateNames[static_cast<int>(it.first)].c_str(), triagens::basics::Json(it.second));
  }
  return result.steal();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       class Query
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not query tracking is disabled globally
////////////////////////////////////////////////////////////////////////////////
          
bool Query::DoDisableQueryTracking = false;

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a query
////////////////////////////////////////////////////////////////////////////////

Query::Query (triagens::arango::ApplicationV8* applicationV8,
              bool contextOwnedByExterior,
              TRI_vocbase_t* vocbase,
              char const* queryString,
              size_t queryLength,
              TRI_json_t* bindParameters,
              TRI_json_t* options,
              QueryPart part)
  : _id(0),
    _applicationV8(applicationV8),
    _vocbase(vocbase),
    _executor(nullptr),
    _context(nullptr),
    _queryString(queryString),
    _queryLength(queryLength),
    _queryJson(),
    _bindParameters(bindParameters),
    _options(options),
    _collections(vocbase),
    _strings(),
    _shortStringStorage(1024),
    _ast(nullptr),
    _profile(nullptr),
    _state(INVALID_STATE),
    _plan(nullptr),
    _parser(nullptr),
    _trx(nullptr),
    _engine(nullptr),
    _maxWarningCount(10),
    _warnings(),
    _part(part),
    _contextOwnedByExterior(contextOwnedByExterior),
    _killed(false),
    _isModificationQuery(false) {

  // std::cout << TRI_CurrentThreadId() << ", QUERY " << this << " CTOR: " << queryString << "\n";

  TRI_ASSERT(_vocbase != nullptr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a query from Json
////////////////////////////////////////////////////////////////////////////////

Query::Query (triagens::arango::ApplicationV8* applicationV8,
              bool contextOwnedByExterior,
              TRI_vocbase_t* vocbase,
              triagens::basics::Json queryStruct,
              TRI_json_t* options,
              QueryPart part)
  : _id(0),
    _applicationV8(applicationV8),
    _vocbase(vocbase),
    _executor(nullptr),
    _context(nullptr),
    _queryString(nullptr),
    _queryLength(0),
    _queryJson(queryStruct),
    _bindParameters(nullptr),
    _options(options),
    _collections(vocbase),
    _strings(),
    _shortStringStorage(1024),
    _ast(nullptr),
    _profile(nullptr),
    _state(INVALID_STATE),
    _plan(nullptr),
    _parser(nullptr),
    _trx(nullptr),
    _engine(nullptr),
    _maxWarningCount(10),
    _warnings(),
    _part(part),
    _contextOwnedByExterior(contextOwnedByExterior),
    _killed(false),
    _isModificationQuery(false) {

  // std::cout << TRI_CurrentThreadId() << ", QUERY " << this << " CTOR (JSON): " << _queryJson.toString() << "\n";

  TRI_ASSERT(_vocbase != nullptr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a query
////////////////////////////////////////////////////////////////////////////////

Query::~Query () {
  // std::cout << TRI_CurrentThreadId() << ", QUERY " << this << " DTOR\r\n";
  cleanupPlanAndEngine(TRI_ERROR_INTERNAL); // abort the transaction

  delete _profile;
  _profile = nullptr;

  if (_options != nullptr) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, _options);
    _options = nullptr;
  }

  delete _executor;
  _executor = nullptr;

  if (_context != nullptr) {
    TRI_ASSERT(! _contextOwnedByExterior);
        
    // unregister transaction and resolver in context
    ISOLATE;
    TRI_GET_GLOBALS();
    auto ctx = static_cast<triagens::arango::V8TransactionContext*>(v8g->_transactionContext);
    if (ctx != nullptr) {
      ctx->unregisterTransaction();
    }

    _applicationV8->exitContext(_context);
    _context = nullptr;
  }

  delete _ast;
  _ast = nullptr;

  // free strings
  for (auto& it : _strings) {
    TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, const_cast<char*>(it));
  }
  // free nodes
  for (auto& it : _nodes) {
    delete it;
  }
  for (auto& it : _graphs) {
    delete it.second;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clone a query
/// note: as a side-effect, this will also create and start a transaction for
/// the query
////////////////////////////////////////////////////////////////////////////////

Query* Query::clone (QueryPart part,
                     bool withPlan) {
  std::unique_ptr<TRI_json_t> options;

  if (_options != nullptr) {
    options.reset(TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, _options));
  }

  std::unique_ptr<Query> clone;

  clone.reset(new Query(_applicationV8, 
                        false,
                        _vocbase,
                        _queryString,
                        _queryLength,
                        nullptr,
                        options.get(),
                        part));
  options.release();

  if (_plan != nullptr) {
    if (withPlan) {
      // clone the existing plan
      clone->setPlan(_plan->clone(*clone));
    }
   
    // clone all variables 
    for (auto& it : _ast->variables()->variables(true)) {
      auto var = _ast->variables()->getVariable(it.first);
      TRI_ASSERT(var != nullptr);
      clone->ast()->variables()->createVariable(var);
    }
  }
    
  if (clone->_plan == nullptr) {
    // initialize an empty plan
    clone->setPlan(new ExecutionPlan(ast()));
  }

  TRI_ASSERT(clone->_trx == nullptr);

  clone->_trx = _trx->clone();  // A daughter transaction which does not
                                // actually lock the collections

  int res = clone->_trx->begin();

  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION_MESSAGE(res, "could not begin transaction");
  }

  return clone.release();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a node to the list of nodes
////////////////////////////////////////////////////////////////////////////////

void Query::addNode (AstNode* node) {
  _nodes.emplace_back(node);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief extract a region from the query
////////////////////////////////////////////////////////////////////////////////

std::string Query::extractRegion (int line, 
                                  int column) const {
  // note: line numbers reported by bison/flex start at 1, columns start at 0
  int currentLine   = 1;
  int currentColumn = 0;

  char c;
  char const* p = _queryString;

  while ((c = *p)) {
    if (currentLine > line || 
        (currentLine >= line && currentColumn >= column)) {
      break;
    }

    if (c == '\n') {
      ++p;
      ++currentLine;
      currentColumn = 0;
    }
    else if (c == '\r') {
      ++p;
      ++currentLine;
      currentColumn = 0;

      // eat a following newline
      if (*p == '\n') {
        ++p;
      }
    }
    else {
      ++currentColumn;
      ++p;
    }
  }

  // p is pointing at the position in the query the parse error occurred at
  TRI_ASSERT(p >= _queryString);

  size_t offset = static_cast<size_t>(p - _queryString);

  static int const   SNIPPET_LENGTH = 32;
  static char const* SNIPPET_SUFFIX = "...";

  if (_queryLength < offset + SNIPPET_LENGTH) {
    // return a copy of the region
    return std::string(_queryString + offset, _queryLength - offset);
  }

  // copy query part
  std::string result(_queryString + offset, SNIPPET_LENGTH);
  result.append(SNIPPET_SUFFIX);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief register an error, with an optional parameter inserted into printf
/// this also makes the query abort
////////////////////////////////////////////////////////////////////////////////

void Query::registerError (int code,
                           char const* details) {

  TRI_ASSERT(code != TRI_ERROR_NO_ERROR);

  if (details == nullptr) {
    THROW_ARANGO_EXCEPTION(code);
  }
  
  THROW_ARANGO_EXCEPTION_PARAMS(code, details);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief register an error with a custom error message
/// this also makes the query abort
////////////////////////////////////////////////////////////////////////////////

void Query::registerErrorCustom (int code,
                                 char const* details) {

  TRI_ASSERT(code != TRI_ERROR_NO_ERROR);

  if (details == nullptr) {
    THROW_ARANGO_EXCEPTION(code);
  }
  
  std::string errorMessage(TRI_errno_string(code));
  errorMessage.append(": ");
  errorMessage.append(details);

  THROW_ARANGO_EXCEPTION_MESSAGE(code, errorMessage);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief register a warning
////////////////////////////////////////////////////////////////////////////////

void Query::registerWarning (int code,
                             char const* details) {

  TRI_ASSERT(code != TRI_ERROR_NO_ERROR);

  if (_warnings.size() > _maxWarningCount) {
    return;
  }

  if (details == nullptr) {
    _warnings.emplace_back(code, TRI_errno_string(code));
  }
  else {
    _warnings.emplace_back(code, details);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prepare an AQL query, this is a preparation for execute, but 
/// execute calls it internally. The purpose of this separate method is
/// to be able to only prepare a query from JSON and then store it in the
/// QueryRegistry.
////////////////////////////////////////////////////////////////////////////////

QueryResult Query::prepare (QueryRegistry* registry) {
  try {
    init();
    enterState(PARSING);

    std::unique_ptr<Parser> parser(new Parser(this));
    std::unique_ptr<ExecutionPlan> plan;
    
    if (_queryString != nullptr) {
      parser->parse(false);
      // put in bind parameters
      parser->ast()->injectBindParameters(_bindParameters);
    }
      
    _isModificationQuery = parser->isModificationQuery();

    // create the transaction object, but do not start it yet
    _trx = new triagens::arango::AqlTransaction(createTransactionContext(), _vocbase, _collections.collections(), _part == PART_MAIN);

    bool planRegisters;

    if (_queryString != nullptr) {
      // we have an AST
      int res = _trx->begin();

      if (res != TRI_ERROR_NO_ERROR) {
        return transactionError(res);
      }

      // optimize the ast
      enterState(AST_OPTIMIZATION);

      parser->ast()->validateAndOptimize();
      // std::cout << "AST: " << triagens::basics::JsonHelper::toString(parser->ast()->toJson(TRI_UNKNOWN_MEM_ZONE, false)) << "\n";

      enterState(PLAN_INSTANTIATION);
      plan.reset(ExecutionPlan::instantiateFromAst(parser->ast()));

      if (plan.get() == nullptr) {
        // oops
        return QueryResult(TRI_ERROR_INTERNAL, "failed to create query execution engine");
      }

      // Run the query optimizer:
      enterState(PLAN_OPTIMIZATION);
      triagens::aql::Optimizer opt(maxNumberOfPlans());
      // getenabled/disabled rules
      opt.createPlans(plan.release(), getRulesFromOptions(), inspectSimplePlans());
      // Now plan and all derived plans belong to the optimizer
      plan.reset(opt.stealBest()); // Now we own the best one again
      planRegisters = true;
    }
    else {   // no queryString, we are instantiating from _queryJson
      enterState(PLAN_INSTANTIATION);
      ExecutionPlan::getCollectionsFromJson(parser->ast(), _queryJson);

      parser->ast()->variables()->fromJson(_queryJson);
      // creating the plan may have produced some collections
      // we need to add them to the transaction now (otherwise the query will fail)

      int res = _trx->addCollectionList(_collections.collections());
      
      if (res == TRI_ERROR_NO_ERROR) {
        res = _trx->begin();
      }

      if (res != TRI_ERROR_NO_ERROR) {
        return transactionError(res);
      }

      // we have an execution plan in JSON format
      plan.reset(ExecutionPlan::instantiateFromJson(parser->ast(), _queryJson));
      if (plan.get() == nullptr) {
        // oops
        return QueryResult(TRI_ERROR_INTERNAL);
      }

      planRegisters = false;
    }
      
    // std::cout << "GOT PLAN:\n" << plan.get()->toJson(parser->ast(), TRI_UNKNOWN_MEM_ZONE, true).toString() << "\n\n";

    TRI_ASSERT(plan.get() != nullptr);
    /* // for debugging of serialization/deserialization . . . * /
    auto JsonPlan = plan->toJson(parser->ast(),TRI_UNKNOWN_MEM_ZONE, true);
    auto JsonString = JsonPlan.toString();
    std::cout << "original plan: \n" << JsonString << "\n";

    auto otherPlan = ExecutionPlan::instantiateFromJson (parser->ast(),
                                                         JsonPlan);
    otherPlan->getCost(); 
    auto otherJsonString =
      otherPlan->toJson(parser->ast(), TRI_UNKNOWN_MEM_ZONE, true).toString(); 
    std::cout << "deserialized plan: \n" << otherJsonString << "\n";
    //TRI_ASSERT(otherJsonString == JsonString); */
    
    // varsUsedLater and varsValid are unordered_sets and so their orders
    // are not the same in the serialized and deserialized plans 

    // return the V8 context
    exitContext();

    enterState(EXECUTION);
    ExecutionEngine* engine(ExecutionEngine::instantiateFromPlan(registry, this, plan.get(), planRegisters));

    // If all went well so far, then we keep _plan, _parser and _trx and
    // return:
    _plan = plan.release();
    _parser = parser.release();
    _engine = engine;
    return QueryResult();
  }
  catch (triagens::basics::Exception const& ex) {
    cleanupPlanAndEngine(ex.code());
    return QueryResult(ex.code(), ex.message() + getStateString());
  }
  catch (std::bad_alloc const&) {
    cleanupPlanAndEngine(TRI_ERROR_OUT_OF_MEMORY);
    return QueryResult(TRI_ERROR_OUT_OF_MEMORY, TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY) + getStateString());
  }
  catch (std::exception const& ex) {
    cleanupPlanAndEngine(TRI_ERROR_INTERNAL);
    return QueryResult(TRI_ERROR_INTERNAL, ex.what() + getStateString());
  }
  catch (...) {
    cleanupPlanAndEngine(TRI_ERROR_INTERNAL);
    return QueryResult(TRI_ERROR_INTERNAL, TRI_errno_string(TRI_ERROR_INTERNAL) + getStateString());
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute an AQL query 
////////////////////////////////////////////////////////////////////////////////

QueryResult Query::execute (QueryRegistry* registry) {
  try {
    bool useQueryCache = canUseQueryCache();
    uint64_t queryStringHash = 0;

    if (useQueryCache) {
      // hash the query
      queryStringHash = hash();

      // check the query cache for an existing result
      auto cacheEntry = triagens::aql::QueryCache::instance()->lookup(_vocbase, queryStringHash, _queryString, _queryLength);
      triagens::aql::QueryCacheResultEntryGuard guard(cacheEntry);

      if (cacheEntry != nullptr) {
        // got a result from the query cache
        QueryResult res(TRI_ERROR_NO_ERROR);
        res.warnings = warningsToJson(TRI_UNKNOWN_MEM_ZONE);
        res.json     = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, cacheEntry->_queryResult);
        res.stats    = nullptr;
        res.cached   = true;

        return res;
      } 
    }

    QueryResult res = prepare(registry);

    if (res.code != TRI_ERROR_NO_ERROR) {
      return res;
    }

    if (useQueryCache && (_isModificationQuery || ! _warnings.empty() || ! _ast->root()->isCacheable())) {
      useQueryCache = false;
    }

    triagens::basics::Json jsonResult(triagens::basics::Json::Array, 16);
    triagens::basics::Json stats;

    // this is the RegisterId our results can be found in
    auto const resultRegister = _engine->resultRegister();
    
    AqlItemBlock* value = nullptr;

    try {
      if (useQueryCache) {
        // iterate over result, return it and store it in query cache
        while (nullptr != (value = _engine->getSome(1, ExecutionBlock::DefaultBatchSize))) {
          auto doc = value->getDocumentCollection(resultRegister);

          size_t const n = value->size();
          // reserve space for n additional results at once
          jsonResult.reserve(n);

          for (size_t i = 0; i < n; ++i) {
            auto val = value->getValueReference(i, resultRegister);

            if (! val.isEmpty()) {
              jsonResult.add(val.toJson(_trx, doc, true)); 
            }
          }
          delete value;
          value = nullptr;
        }

        if (_warnings.empty()) {
          // finally store the generated result in the query cache
          QueryCache::instance()->store(
            _vocbase, 
            queryStringHash, 
            _queryString, 
            _queryLength, 
            TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, jsonResult.json()), 
            _trx->collectionNames()
          );
        }
      }
      else {
        // iterate over result and return it
        while (nullptr != (value = _engine->getSome(1, ExecutionBlock::DefaultBatchSize))) {
          auto doc = value->getDocumentCollection(resultRegister);

          size_t const n = value->size();
          // reserve space for n additional results at once
          jsonResult.reserve(n);

          for (size_t i = 0; i < n; ++i) {
            auto val = value->getValueReference(i, resultRegister);

            if (! val.isEmpty()) {
              jsonResult.add(val.toJson(_trx, doc, true)); 
            }
          }
          delete value;
          value = nullptr;
        }
      }
    }
    catch (...) {
      delete value;
      throw;
    }

    stats = _engine->_stats.toJson();

    _trx->commit();
    
    cleanupPlanAndEngine(TRI_ERROR_NO_ERROR);

    enterState(FINALIZATION); 

    QueryResult result(TRI_ERROR_NO_ERROR);
    result.warnings = warningsToJson(TRI_UNKNOWN_MEM_ZONE);
    result.json     = jsonResult.steal();
    result.stats    = stats.steal(); 

    if (_profile != nullptr && profiling()) {
      result.profile = _profile->toJson(TRI_UNKNOWN_MEM_ZONE);
    }

    return result;
  }
  catch (triagens::basics::Exception const& ex) {
    cleanupPlanAndEngine(ex.code());
    return QueryResult(ex.code(), ex.message() + getStateString());
  }
  catch (std::bad_alloc const&) {
    cleanupPlanAndEngine(TRI_ERROR_OUT_OF_MEMORY);
    return QueryResult(TRI_ERROR_OUT_OF_MEMORY, TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY) + getStateString());
  }
  catch (std::exception const& ex) {
    cleanupPlanAndEngine(TRI_ERROR_INTERNAL);
    return QueryResult(TRI_ERROR_INTERNAL, ex.what() + getStateString());
  }
  catch (...) {
    cleanupPlanAndEngine(TRI_ERROR_INTERNAL);
    return QueryResult(TRI_ERROR_INTERNAL, TRI_errno_string(TRI_ERROR_INTERNAL) + getStateString());
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute an AQL query 
/// may only be called with an active V8 handle scope
////////////////////////////////////////////////////////////////////////////////

QueryResultV8 Query::executeV8 (v8::Isolate* isolate, 
                                QueryRegistry* registry) {
  try {
    bool useQueryCache = canUseQueryCache();
    uint64_t queryStringHash = 0;

    if (useQueryCache) {
      // hash the query
      queryStringHash = hash();

      // check the query cache for an existing result
      auto cacheEntry = triagens::aql::QueryCache::instance()->lookup(_vocbase, queryStringHash, _queryString, _queryLength);
      triagens::aql::QueryCacheResultEntryGuard guard(cacheEntry);

      if (cacheEntry != nullptr) {
        // got a result from the query cache
        QueryResultV8 res(TRI_ERROR_NO_ERROR);
        res.result = v8::Handle<v8::Array>::Cast(TRI_ObjectJson(isolate, cacheEntry->_queryResult));
        res.cached = true;
        return res;
      } 
    }

    QueryResultV8 res = prepare(registry);

    if (res.code != TRI_ERROR_NO_ERROR) {
      return res;
    }

    if (useQueryCache && (_isModificationQuery || ! _warnings.empty() || ! _ast->root()->isCacheable())) {
      useQueryCache = false;
    }

    QueryResultV8 result(TRI_ERROR_NO_ERROR);
    result.result = v8::Array::New(isolate);
    triagens::basics::Json stats;
    
    // this is the RegisterId our results can be found in
    auto const resultRegister = _engine->resultRegister();
    AqlItemBlock* value = nullptr;

    try {
      if (useQueryCache) {
        // iterate over result, return it and store it in query cache
        std::unique_ptr<TRI_json_t> cacheResult(TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE));

        uint32_t j = 0;
        while (nullptr != (value = _engine->getSome(1, ExecutionBlock::DefaultBatchSize))) {
          auto doc = value->getDocumentCollection(resultRegister);

          size_t const n = value->size();
          
          for (size_t i = 0; i < n; ++i) {
            auto val = value->getValueReference(i, resultRegister);

            if (! val.isEmpty()) {
              result.result->Set(j++, val.toV8(isolate, _trx, doc));

              auto json = val.toJson(_trx, doc, true);
              TRI_PushBack3ArrayJson(TRI_UNKNOWN_MEM_ZONE, cacheResult.get(), json.steal());
            }
          }
          delete value;
          value = nullptr;
        }

        if (_warnings.empty()) {
          // finally store the generated result in the query cache
          QueryCache::instance()->store(
            _vocbase, 
            queryStringHash, 
            _queryString, 
            _queryLength, 
            cacheResult.get(), 
            _trx->collectionNames()
          );
          cacheResult.release();
        }
      }
      else {
        // iterate over result and return it
        uint32_t j = 0;
        while (nullptr != (value = _engine->getSome(1, ExecutionBlock::DefaultBatchSize))) {
          auto doc = value->getDocumentCollection(resultRegister);

          size_t const n = value->size();
          
          for (size_t i = 0; i < n; ++i) {
            auto val = value->getValueReference(i, resultRegister);

            if (! val.isEmpty()) {
              result.result->Set(j++, val.toV8(isolate, _trx, doc));
            }
          }
          delete value;
          value = nullptr;
        }
      }
    }
    catch (...) {
      delete value;
      throw;
    }

    stats = _engine->_stats.toJson();

    _trx->commit();
    
    cleanupPlanAndEngine(TRI_ERROR_NO_ERROR);

    enterState(FINALIZATION); 

    result.warnings = warningsToJson(TRI_UNKNOWN_MEM_ZONE);
    result.stats    = stats.steal(); 

    if (_profile != nullptr && profiling()) {
      result.profile = _profile->toJson(TRI_UNKNOWN_MEM_ZONE);
    }

    return result;
  }
  catch (triagens::basics::Exception const& ex) {
    cleanupPlanAndEngine(ex.code());
    return QueryResultV8(ex.code(), ex.message() + getStateString());
  }
  catch (std::bad_alloc const&) {
    cleanupPlanAndEngine(TRI_ERROR_OUT_OF_MEMORY);
    return QueryResultV8(TRI_ERROR_OUT_OF_MEMORY, TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY) + getStateString());
  }
  catch (std::exception const& ex) {
    cleanupPlanAndEngine(TRI_ERROR_INTERNAL);
    return QueryResultV8(TRI_ERROR_INTERNAL, ex.what() + getStateString());
  }
  catch (...) {
    cleanupPlanAndEngine(TRI_ERROR_INTERNAL);
    return QueryResult(TRI_ERROR_INTERNAL, TRI_errno_string(TRI_ERROR_INTERNAL) + getStateString());
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parse an AQL query
////////////////////////////////////////////////////////////////////////////////

QueryResult Query::parse () {
  try {
    init();
    Parser parser(this);
    return parser.parse(true);
  }
  catch (triagens::basics::Exception const& ex) {
    return QueryResult(ex.code(), ex.message());
  }
  catch (std::bad_alloc const&) {
    cleanupPlanAndEngine(TRI_ERROR_OUT_OF_MEMORY);
    return QueryResult(TRI_ERROR_OUT_OF_MEMORY, TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY));
  }
  catch (std::exception const& ex) {
    cleanupPlanAndEngine(TRI_ERROR_INTERNAL);
    return QueryResult(TRI_ERROR_INTERNAL, ex.what());
  }
  catch (...) {
    return QueryResult(TRI_ERROR_OUT_OF_MEMORY, TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief explain an AQL query
////////////////////////////////////////////////////////////////////////////////

QueryResult Query::explain () {
  try {
    init();
    enterState(PARSING);

    Parser parser(this);

    parser.parse(true);
    // put in bind parameters
    parser.ast()->injectBindParameters(_bindParameters);

    enterState(AST_OPTIMIZATION);
    // optimize and validate the ast
    parser.ast()->validateAndOptimize();
    // std::cout << "AST: " << triagens::basics::JsonHelper::toString(parser.ast()->toJson(TRI_UNKNOWN_MEM_ZONE)) << "\n";

    // create the transaction object, but do not start it yet
    _trx = new triagens::arango::AqlTransaction(createTransactionContext(), _vocbase, _collections.collections(), true);

    // we have an AST
    int res = _trx->begin();

    if (res != TRI_ERROR_NO_ERROR) {
      return transactionError(res);
    }

    enterState(PLAN_INSTANTIATION);
    ExecutionPlan* plan = ExecutionPlan::instantiateFromAst(parser.ast());

    if (plan == nullptr) {
      // oops
      return QueryResult(TRI_ERROR_INTERNAL);
    }

    // Run the query optimizer:
    enterState(PLAN_OPTIMIZATION);
    triagens::aql::Optimizer opt(maxNumberOfPlans());
    // get enabled/disabled rules
    opt.createPlans(plan, getRulesFromOptions(), inspectSimplePlans());
      

    enterState(FINALIZATION);
      
    QueryResult result(TRI_ERROR_NO_ERROR);
    QueryRegistry localRegistry;

    if (allPlans()) {
      triagens::basics::Json out(Json::Array);

      auto plans = opt.getPlans();
      for (auto& it : plans) {
        TRI_ASSERT(it != nullptr);

        it->findVarUsage();
        it->planRegisters();
        out.add(it->toJson(parser.ast(), TRI_UNKNOWN_MEM_ZONE, verbosePlans()));
      }
      result.json = out.steal();
    }
    else {
      // Now plan and all derived plans belong to the optimizer
      std::unique_ptr<ExecutionPlan> bestPlan(opt.stealBest()); // Now we own the best one again
      TRI_ASSERT(bestPlan != nullptr);

      bestPlan->findVarUsage();
      bestPlan->planRegisters();
      result.json = bestPlan->toJson(parser.ast(), TRI_UNKNOWN_MEM_ZONE, verbosePlans()).steal(); 
    }

    _trx->commit();
      
    result.warnings = warningsToJson(TRI_UNKNOWN_MEM_ZONE);
    result.stats = opt._stats.toJson(TRI_UNKNOWN_MEM_ZONE);

    return result;
  }
  catch (triagens::basics::Exception const& ex) {
    return QueryResult(ex.code(), ex.message() + getStateString());
  }
  catch (std::bad_alloc const&) {
    return QueryResult(TRI_ERROR_OUT_OF_MEMORY, TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY) + getStateString());
  }
  catch (std::exception const& ex) {
    return QueryResult(TRI_ERROR_INTERNAL, ex.what() + getStateString());
  }
  catch (...) {
    return QueryResult(TRI_ERROR_INTERNAL, TRI_errno_string(TRI_ERROR_INTERNAL) + getStateString());
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get v8 executor
////////////////////////////////////////////////////////////////////////////////

Executor* Query::executor () {
  if (_executor == nullptr) {
    // the executor is a singleton per query
    _executor = new Executor(literalSizeThreshold());
  }

  TRI_ASSERT(_executor != nullptr);
  return _executor;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief register a string
/// the string is freed when the query is destroyed
////////////////////////////////////////////////////////////////////////////////

char* Query::registerString (char const* p, 
                             size_t length) {

  if (p == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  if (length == 0) {
    // optimization for the empty string
    return const_cast<char*>(EmptyString);
  }

  if (length < ShortStringStorage::MaxStringLength) {
    return _shortStringStorage.registerString(p, length); 
  }

  char* copy = TRI_DuplicateString2Z(TRI_UNKNOWN_MEM_ZONE, p, length);

  if (copy == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  try {
    _strings.emplace_back(copy);
  }
  catch (...) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  return copy;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief register a string
/// the string is freed when the query is destroyed
////////////////////////////////////////////////////////////////////////////////

char* Query::registerString (std::string const& p) {
  return registerString(p.c_str(), p.length());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief register a potentially UTF-8-escaped string
/// the string is freed when the query is destroyed
////////////////////////////////////////////////////////////////////////////////

char* Query::registerEscapedString (char const* p, 
                                    size_t length,
                                    size_t& outLength) {

  if (p == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  if (length == 0) {
    // optimization for the empty string
    outLength = 0;
    return const_cast<char*>(EmptyString);
  }

  char* copy = TRI_UnescapeUtf8String(TRI_UNKNOWN_MEM_ZONE, p, length, &outLength);

  if (copy == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  try {
    _strings.emplace_back(copy);
  }
  catch (...) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  return copy;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief enter a V8 context
////////////////////////////////////////////////////////////////////////////////

void Query::enterContext () {
  if (! _contextOwnedByExterior) {
    if (_context == nullptr) {
      _context = _applicationV8->enterContext(_vocbase, false);

      if (_context == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "cannot enter V8 context");
      }
    
      // register transaction and resolver in context
      TRI_ASSERT(_trx != nullptr);

      ISOLATE;
      TRI_GET_GLOBALS();
      auto ctx = static_cast<triagens::arango::V8TransactionContext*>(v8g->_transactionContext);
      if (ctx != nullptr) {
        ctx->registerTransaction(_trx->getInternals());
      }
    }

    TRI_ASSERT(_context != nullptr);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a V8 context
////////////////////////////////////////////////////////////////////////////////

void Query::exitContext () {
  if (! _contextOwnedByExterior) {
    if (_context != nullptr) {
      // unregister transaction and resolver in context
      ISOLATE;
      TRI_GET_GLOBALS();
      auto ctx = static_cast<triagens::arango::V8TransactionContext*>(v8g->_transactionContext);
      if (ctx != nullptr) {
        ctx->unregisterTransaction();
      }

      _applicationV8->exitContext(_context);
      _context = nullptr;
    }
    TRI_ASSERT(_context == nullptr);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns statistics for current query.
////////////////////////////////////////////////////////////////////////////////

triagens::basics::Json Query::getStats() {
  if (_engine) {
    return _engine->_stats.toJson();
  }
  return ExecutionStats::toJsonStatic();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fetch a boolean value from the options
////////////////////////////////////////////////////////////////////////////////

bool Query::getBooleanOption (char const* option, bool defaultValue) const {  
  if (! TRI_IsObjectJson(_options)) {
    return defaultValue;
  }

  TRI_json_t const* valueJson = TRI_LookupObjectJson(_options, option);
  if (! TRI_IsBooleanJson(valueJson)) {
    return defaultValue;
  }

  return valueJson->_value._boolean;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert the list of warnings to JSON
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* Query::warningsToJson (TRI_memory_zone_t* zone) const {
  if (_warnings.empty()) {
    return nullptr;
  }

  size_t const n = _warnings.size();
  TRI_json_t* json = TRI_CreateArrayJson(zone, n);

  if (json != nullptr) {
    for (size_t i = 0; i < n; ++i) {
      TRI_json_t* error = TRI_CreateObjectJson(zone, 2);

      if (error != nullptr) {
        TRI_Insert3ObjectJson(zone, error, "code", TRI_CreateNumberJson(zone, static_cast<double>(_warnings[i].first)));
        TRI_Insert3ObjectJson(zone, error, "message", TRI_CreateStringCopyJson(zone, _warnings[i].second.c_str(), _warnings[i].second.size()));

        TRI_PushBack3ArrayJson(zone, json, error);
      }
    }
  }

  return json;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes the query
////////////////////////////////////////////////////////////////////////////////

void Query::init () {
  if (_id != 0) {
    // already called
    return;
  }
   
  TRI_ASSERT(_id == 0);
  TRI_ASSERT(_ast == nullptr);
 
  _id = TRI_NextQueryIdVocBase(_vocbase);

  _profile = new Profile(this);
  enterState(INITIALIZATION);
  
  _ast = new Ast(this);
  _nodes.reserve(32);
  _strings.reserve(32);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief calculate a hash value for the query and bind parameters
////////////////////////////////////////////////////////////////////////////////

uint64_t Query::hash () const {
  // hash the query string first
  uint64_t hash = triagens::aql::QueryCache::instance()->hashQueryString(_queryString, _queryLength);

  // handle "fullCount" option. if this option is set, the query result will
  // be different to when it is not set! 
  if (getBooleanOption("fullcount", false)) {
    hash = fasthash64(TRI_CHAR_LENGTH_PAIR("fullcount:true"), hash);
  }
  else {
    hash = fasthash64(TRI_CHAR_LENGTH_PAIR("fullcount:false"), hash);
  }

  // handle "count" option
  if (getBooleanOption("count", false)) {
    hash = fasthash64(TRI_CHAR_LENGTH_PAIR("count:true"), hash);
  }
  else {
    hash = fasthash64(TRI_CHAR_LENGTH_PAIR("count:false"), hash);
  }

  // blend query hash with bind parameters
  return hash ^ _bindParameters.hash(); 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the query cache can be used for the query
////////////////////////////////////////////////////////////////////////////////

bool Query::canUseQueryCache () const {
  if (_queryString == nullptr || _queryLength < 8) {
    return false;
  }

  auto queryCacheMode = QueryCache::instance()->mode(); 

  if (queryCacheMode == CACHE_ALWAYS_ON && getBooleanOption("cache", true)) {
    // cache mode is set to always on... query can still be excluded from cache by 
    // setting `cache` attribute to false.

    // cannot use query cache on a coordinator at the moment
    return ! triagens::arango::ServerState::instance()->isRunningInCluster();
  }
  else if (queryCacheMode == CACHE_ON_DEMAND && getBooleanOption("cache", false)) {
    // cache mode is set to demand... query will only be cached if `cache`
    // attribute is set to false
    
    // cannot use query cache on a coordinator at the moment
    return ! triagens::arango::ServerState::instance()->isRunningInCluster();
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fetch a numeric value from the options
////////////////////////////////////////////////////////////////////////////////

double Query::getNumericOption (char const* option, double defaultValue) const {  
  if (! TRI_IsObjectJson(_options)) {
    return defaultValue;
  }

  TRI_json_t const* valueJson = TRI_LookupObjectJson(_options, option);
  if (! TRI_IsNumberJson(valueJson)) {
    return defaultValue;
  }

  return valueJson->_value._number;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief neatly format transaction error to the user.
////////////////////////////////////////////////////////////////////////////////

QueryResult Query::transactionError (int errorCode) const {
  std::string err(TRI_errno_string(errorCode));

  auto detail = _trx->getErrorData();
  if (detail.size() > 0) {
    err += std::string(" (") + detail + std::string(")");
  }

  if (_queryString != nullptr && verboseErrors()) {
    err += std::string("\nwhile executing:\n") + _queryString + std::string("\n");
  }

  return QueryResult(errorCode, err);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read the "optimizer.inspectSimplePlans" section from the options
////////////////////////////////////////////////////////////////////////////////

bool Query::inspectSimplePlans () const {
  if (! TRI_IsObjectJson(_options)) {
    return true; // default
  }
  
  TRI_json_t const* optJson = TRI_LookupObjectJson(_options, "optimizer");

  if (! TRI_IsObjectJson(optJson)) {
    return true; // default
  }
  
  TRI_json_t const* j = TRI_LookupObjectJson(optJson, "inspectSimplePlans");
  if (TRI_IsBooleanJson(j)) {
    return j->_value._boolean;
  }
  return true; // default;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read the "optimizer.rules" section from the options
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> Query::getRulesFromOptions () const {
  std::vector<std::string> rules;

  if (! TRI_IsObjectJson(_options)) {
    return rules;
  }
  
  TRI_json_t const* optJson = TRI_LookupObjectJson(_options, "optimizer");

  if (! TRI_IsObjectJson(optJson)) {
    return rules;
  }
  
  TRI_json_t const* rulesJson = TRI_LookupObjectJson(optJson, "rules");

  if (! TRI_IsArrayJson(rulesJson)) {
    return rules;
  }
        
  size_t const n = TRI_LengthArrayJson(rulesJson);

  for (size_t i = 0; i < n; ++i) {
    TRI_json_t const* rule = static_cast<TRI_json_t const*>(TRI_AtVector(&rulesJson->_value._objects, i));

    if (TRI_IsStringJson(rule)) {
      rules.emplace_back(rule->_value._string.data, rule->_value._string.length - 1);
    }
  }

  return rules;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief enter a new state
////////////////////////////////////////////////////////////////////////////////

void Query::enterState (ExecutionState state) {
  if (_profile != nullptr) {
    // record timing for previous state
    _profile->setDone(_state);
  }
  
  // and adjust the state
  _state = state;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a description of the query's current state
////////////////////////////////////////////////////////////////////////////////

std::string Query::getStateString () const {
  return std::string(" (while " + StateNames[_state] + ")");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cleanup plan and engine for current query
////////////////////////////////////////////////////////////////////////////////

void Query::cleanupPlanAndEngine (int errorCode) {
  if (_engine != nullptr) {
    try {
      _engine->shutdown(errorCode); 
    }
    catch (...) {
      // shutdown may fail but we must not throw here 
      // (we're also called from the destructor)
    }
    delete _engine;
    _engine = nullptr;
  }

  if (_trx != nullptr) {
    // If the transaction was not committed, it is automatically aborted
    delete _trx;
    _trx = nullptr;
  }

  if (_parser != nullptr) {
    delete _parser;
    _parser = nullptr;
  }

  if (_plan != nullptr) {
    delete _plan;
    _plan = nullptr;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the plan for the query
////////////////////////////////////////////////////////////////////////////////

void Query::setPlan (ExecutionPlan *plan) {
  if (_plan != nullptr) {
    delete _plan;
  }
  _plan = plan;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a TransactionContext
////////////////////////////////////////////////////////////////////////////////

triagens::arango::TransactionContext* Query::createTransactionContext () {
  if (_contextOwnedByExterior) {
    // we can use v8
    return new triagens::arango::V8TransactionContext(true);
  }

  return new triagens::arango::StandaloneTransactionContext();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a TransactionContext
////////////////////////////////////////////////////////////////////////////////

Graph const* Query::lookupGraphByName (std::string &name) {
  auto it = _graphs.find(name);
  if (it != _graphs.end()) {
    return it->second;
  }

  auto g = triagens::arango::lookupGraphByName (_vocbase, name);
  if (g != nullptr) {
    _graphs.emplace(name, g);
  }
  return g;
}


// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
