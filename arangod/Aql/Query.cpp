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
#include "Basics/JsonHelper.h"
#include "Basics/json.h"
#include "Basics/tri-strings.h"
#include "Cluster/ServerState.h"
#include "Utils/AqlTransaction.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/Exception.h"
#include "Utils/StandaloneTransactionContext.h"
#include "Utils/V8TransactionContext.h"
#include "V8Server/ApplicationV8.h"
#include "VocBase/vocbase.h"

using namespace triagens::aql;
using Json = triagens::basics::Json;

// -----------------------------------------------------------------------------
// --SECTION--                                               static const values
// -----------------------------------------------------------------------------

static std::string StateNames[] = {
  "initializing",           // INITIALIZATION 
  "parsing",                // PARSING
  "optimizing ast",         // AST_OPTIMIZATION
  "instanciating plan",     // PLAN_INSTANCIATION
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
      
Profile::Profile () 
  : results(static_cast<size_t>(INVALID_STATE)),
    stamp(TRI_microtime()) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief enter a state
////////////////////////////////////////////////////////////////////////////////

void Profile::enter (ExecutionState state) {
  double const now = TRI_microtime();

  if (state != ExecutionState::INVALID_STATE) {
    // record duration of state
    results.push_back(std::make_pair(state, now - stamp)); 
  }

  // set timestamp
  stamp = now;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert the profile to JSON
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* Profile::toJson (TRI_memory_zone_t*) {
  triagens::basics::Json result(triagens::basics::Json::Array);
  for (auto& it : results) {
    result.set(StateNames[static_cast<int>(it.first)].c_str(), triagens::basics::Json(it.second));
  }
  return result.steal();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       class Query
// -----------------------------------------------------------------------------

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
  : _applicationV8(applicationV8),
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
    _contextOwnedByExterior(contextOwnedByExterior) {

  // std::cout << TRI_CurrentThreadId() << ", QUERY " << this << " CTOR: " << queryString << "\r\n";
  TRI_ASSERT(_vocbase != nullptr);

  if (profiling()) {
    _profile = new Profile;
  }
  enterState(INITIALIZATION);
  
  _ast = new Ast(this);

  _nodes.reserve(32);
  _strings.reserve(32);
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
  : _applicationV8(applicationV8),
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
    _contextOwnedByExterior(contextOwnedByExterior) {

  TRI_ASSERT(_vocbase != nullptr);

  if (profiling()) {
    _profile = new Profile;
  }
  enterState(INITIALIZATION);

  _nodes.reserve(32);

  _ast = new Ast(this);
  _strings.reserve(32);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a query
////////////////////////////////////////////////////////////////////////////////

Query::~Query () {
  // std::cout << TRI_CurrentThreadId() << ", QUERY " << this << " DTOR\r\n";
  cleanupPlanAndEngine(TRI_ERROR_INTERNAL); // abort the transaction

  if (_profile != nullptr) {
    delete _profile;
    _profile = nullptr;
  }

  if (_options != nullptr) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, _options);
    _options = nullptr;
  }

  if (_executor != nullptr) {
    delete _executor;
    _executor = nullptr;
  }

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

  if (_ast != nullptr) {
    delete _ast;
    _ast = nullptr;
  }

  // free strings
  for (auto it = _strings.begin(); it != _strings.end(); ++it) {
    TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, const_cast<char*>(*it));
  }
  // free nodes
  for (auto it = _nodes.begin(); it != _nodes.end(); ++it) {
    delete (*it);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clone a query
/// note: as a side-effect, this will also create and start a transaction for
/// the query
////////////////////////////////////////////////////////////////////////////////

Query* Query::clone (QueryPart part,
                     bool withPlan) {
  TRI_json_t* options = nullptr;

  if (_options != nullptr) {
    options = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, _options);
  }

  std::unique_ptr<Query> clone;

  try {
    clone.reset(new Query(_applicationV8, 
                          false,
                          _vocbase,
                          _queryString,
                          _queryLength,
                          nullptr,
                          options,
                          part));
  }
  catch (...) {
    if (options != nullptr) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, options);
    }
    throw;
  }

  if (_plan != nullptr) {
    if (withPlan) {
      // clone the existing plan
      clone->setPlan(_plan->clone(*clone));
    }
   
    // clone all variables 
    for (auto it : _ast->variables()->variables(true)) {
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
  _nodes.push_back(node);
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
/// @brief register an error
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
/// @brief register a warning
////////////////////////////////////////////////////////////////////////////////

void Query::registerWarning (int code,
                             char const* details) {

  TRI_ASSERT(code != TRI_ERROR_NO_ERROR);

  if (_warnings.size() > _maxWarningCount) {
    return;
  }

  if (details == nullptr) {
    _warnings.emplace_back(std::make_pair(code, TRI_errno_string(code)));
  }
  else {
    _warnings.emplace_back(std::make_pair(code, details));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prepare an AQL query, this is a preparation for execute, but 
/// execute calls it internally. The purpose of this separate method is
/// to be able to only prepare a query from JSON and then store it in the
/// QueryRegistry.
////////////////////////////////////////////////////////////////////////////////

QueryResult Query::prepare (QueryRegistry* registry) {
  enterState(PARSING);

  try {
    std::unique_ptr<Parser> parser(new Parser(this));
    std::unique_ptr<ExecutionPlan> plan;

    if (_queryString != nullptr) {
      parser->parse(false);
      // put in bind parameters
      parser->ast()->injectBindParameters(_bindParameters);

      // optimize the ast
      enterState(AST_OPTIMIZATION);
      parser->ast()->optimize();
      // std::cout << "AST: " << triagens::basics::JsonHelper::toString(parser->ast()->toJson(TRI_UNKNOWN_MEM_ZONE, false)) << "\n";
    }

    // create the transaction object, but do not start it yet
    auto trx = new triagens::arango::AqlTransaction(createTransactionContext(), _vocbase, _collections.collections(), _part == PART_MAIN);
    _trx = trx;   // Save the transaction in our object

    bool planRegisters;

    if (_queryString != nullptr) {
      // we have an AST
      int res = _trx->begin();

      if (res != TRI_ERROR_NO_ERROR) {
        return transactionError(res);
      }

      enterState(PLAN_INSTANCIATION);
      plan.reset(ExecutionPlan::instanciateFromAst(parser->ast()));
      if (plan.get() == nullptr) {
        // oops
        return QueryResult(TRI_ERROR_INTERNAL, "failed to create query execution engine");
      }

      // Run the query optimiser:
      enterState(PLAN_OPTIMIZATION);
      triagens::aql::Optimizer opt(maxNumberOfPlans());
      // getenabled/disabled rules
      opt.createPlans(plan.release(), getRulesFromOptions(), inspectSimplePlans());
      // Now plan and all derived plans belong to the optimizer
      plan.reset(opt.stealBest()); // Now we own the best one again
      planRegisters = true;
    }
    else {   // no queryString, we are instanciating from _queryJson
      enterState(PLAN_INSTANCIATION);
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
      plan.reset(ExecutionPlan::instanciateFromJson(parser->ast(), _queryJson));
      if (plan.get() == nullptr) {
        // oops
        return QueryResult(TRI_ERROR_INTERNAL);
      }

      // std::cout << "GOT PLAN:\n" << plan.get()->toJson(parser->ast(), TRI_UNKNOWN_MEM_ZONE, true).toString() << "\n\n";
      planRegisters = false;
    }

    TRI_ASSERT(plan.get() != nullptr);
    /* // for debugging of serialisation/deserialisation . . . * /
    auto JsonPlan = plan->toJson(parser->ast(),TRI_UNKNOWN_MEM_ZONE, true);
    auto JsonString = JsonPlan.toString();
    std::cout << "original plan: \n" << JsonString << "\n";

    auto otherPlan = ExecutionPlan::instanciateFromJson (parser->ast(),
                                                         JsonPlan);
    otherPlan->getCost(); 
    auto otherJsonString =
      otherPlan->toJson(parser->ast(), TRI_UNKNOWN_MEM_ZONE, true).toString(); 
    std::cout << "deserialised plan: \n" << otherJsonString << "\n";
    //TRI_ASSERT(otherJsonString == JsonString); */
    
    // varsUsedLater and varsValid are unordered_sets and so their orders
    // are not the same in the serialised and deserialised plans 

    // return the V8 context
    exitContext();

    enterState(EXECUTION);
    ExecutionEngine* engine(ExecutionEngine::instanciateFromPlan(registry, this, plan.get(), planRegisters));

    // If all went well so far, then we keep _plan, _parser and _trx and
    // return:
    _plan = plan.release();
    _parser = parser.release();
    _engine = engine;
    return QueryResult();
  }
  catch (triagens::arango::Exception const& ex) {
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
  // Now start the execution:
  try {
    QueryResult res = prepare(registry);
    if (res.code != TRI_ERROR_NO_ERROR) {
      return res;
    }

    triagens::basics::Json jsonResult(triagens::basics::Json::List, 16);
    triagens::basics::Json stats;

    AqlItemBlock* value;

    while (nullptr != (value = _engine->getSome(1, ExecutionBlock::DefaultBatchSize))) {
      auto doc = value->getDocumentCollection(0);
      size_t const n = value->size();
      // reserve space for n additional results at once
      jsonResult.reserve(n);

      for (size_t i = 0; i < n; ++i) {
        AqlValue val = value->getValue(i, 0);

        if (! val.isEmpty()) {
          jsonResult.add(val.toJson(_trx, doc)); 
        }
      }
      delete value;
    }

    stats = _engine->_stats.toJson();

    _trx->commit();
    
    cleanupPlanAndEngine(TRI_ERROR_NO_ERROR);

    enterState(FINALIZATION); 

    QueryResult result(TRI_ERROR_NO_ERROR);
    result.warnings = warningsToJson(TRI_UNKNOWN_MEM_ZONE);
    result.json     = jsonResult.steal();
    result.stats    = stats.steal(); 

    if (_profile != nullptr) {
      result.profile = _profile->toJson(TRI_UNKNOWN_MEM_ZONE);
    }

    return result;
  }
  catch (triagens::arango::Exception const& ex) {
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

QueryResultV8 Query::executeV8 (v8::Isolate* isolate, QueryRegistry* registry) {

  // Now start the execution:
  try {
    QueryResultV8 res = prepare(registry);
    if (res.code != TRI_ERROR_NO_ERROR) {
      return res;
    }

    uint32_t j = 0;
    QueryResultV8 result(TRI_ERROR_NO_ERROR);
    result.result  = v8::Array::New(isolate);
    triagens::basics::Json stats;

    AqlItemBlock* value;

    while (nullptr != (value = _engine->getSome(1, ExecutionBlock::DefaultBatchSize))) {
      auto doc = value->getDocumentCollection(0);
      size_t const n = value->size();
      // reserve space for n additional results at once
      /// json.reserve(n);
      
      for (size_t i = 0; i < n; ++i) {
        AqlValue val = value->getValue(i, 0);

        if (! val.isEmpty()) {
          result.result->Set(j++, val.toV8(isolate, _trx, doc)); 
        }
      }
      delete value;
    }

    stats = _engine->_stats.toJson();

    _trx->commit();
    
    cleanupPlanAndEngine(TRI_ERROR_NO_ERROR);

    enterState(FINALIZATION); 

    result.warnings = warningsToJson(TRI_UNKNOWN_MEM_ZONE);
    result.stats    = stats.steal(); 

    if (_profile != nullptr) {
      result.profile = _profile->toJson(TRI_UNKNOWN_MEM_ZONE);
    }

    return result;
  }
  catch (triagens::arango::Exception const& ex) {
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
    Parser parser(this);
    return parser.parse(true);
  }
  catch (triagens::arango::Exception const& ex) {
    return QueryResult(ex.code(), ex.message());
  }
  catch (...) {
    return QueryResult(TRI_ERROR_OUT_OF_MEMORY, TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief explain an AQL query
////////////////////////////////////////////////////////////////////////////////

QueryResult Query::explain () {
  enterState(PARSING);

  try {
    ExecutionPlan* plan = nullptr;
    Parser parser(this);

    parser.parse(true);
    // put in bind parameters
    parser.ast()->injectBindParameters(_bindParameters);

    enterState(AST_OPTIMIZATION);
    // optimize the ast
    parser.ast()->optimize();
    // std::cout << "AST: " << triagens::basics::JsonHelper::toString(parser.ast()->toJson(TRI_UNKNOWN_MEM_ZONE)) << "\n";

    // create the transaction object, but do not start it yet
    auto trx = new triagens::arango::AqlTransaction(createTransactionContext(), _vocbase, _collections.collections(), true);
    _trx = trx;  // save the pointer in this

    // we have an AST
    int res = _trx->begin();

    if (res != TRI_ERROR_NO_ERROR) {
      return transactionError(res);
    }

    enterState(PLAN_INSTANCIATION);
    plan = ExecutionPlan::instanciateFromAst(parser.ast());
    if (plan == nullptr) {
      // oops
      return QueryResult(TRI_ERROR_INTERNAL);
    }

    // Run the query optimiser:
    enterState(PLAN_OPTIMIZATION);
    triagens::aql::Optimizer opt(maxNumberOfPlans());
    // get enabled/disabled rules
    opt.createPlans(plan, getRulesFromOptions(), inspectSimplePlans());
      

    enterState(FINALIZATION);
      
    QueryResult result(TRI_ERROR_NO_ERROR);
    QueryRegistry localRegistry;

    if (allPlans()) {
      triagens::basics::Json out(Json::List);

      auto plans = opt.getPlans();
      for (auto it : plans) {
        TRI_ASSERT(it != nullptr);

        it->findVarUsage();
        it->planRegisters();
        out.add(it->toJson(parser.ast(), TRI_UNKNOWN_MEM_ZONE, verbosePlans()));
      }
      result.json = out.steal();
    }
    else {
      // Now plan and all derived plans belong to the optimizer
      plan = opt.stealBest(); // Now we own the best one again
      TRI_ASSERT(plan != nullptr);
      plan->findVarUsage();
      plan->planRegisters();
      result.json = plan->toJson(parser.ast(), TRI_UNKNOWN_MEM_ZONE, verbosePlans()).steal(); 
      delete plan;
    }

    _trx->commit();
      
    result.warnings = warningsToJson(TRI_UNKNOWN_MEM_ZONE);
    result.stats = opt._stats.toJson(TRI_UNKNOWN_MEM_ZONE);

    return result;
  }
  catch (triagens::arango::Exception const& ex) {
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
    _executor = new Executor;
  }

  TRI_ASSERT(_executor != nullptr);
  return _executor;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief register a string
/// the string is freed when the query is destroyed
////////////////////////////////////////////////////////////////////////////////

char* Query::registerString (char const* p, 
                             size_t length,
                             bool mustUnescape) {

  if (p == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  if (length == 0) {
    static char const* empty = "";
    // optimisation for the empty string
    return const_cast<char*>(empty);
  }

  char* copy = nullptr;
  if (mustUnescape) {
    size_t outLength;
    copy = TRI_UnescapeUtf8StringZ(TRI_UNKNOWN_MEM_ZONE, p, length, &outLength);
  }
  else {
    copy = TRI_DuplicateString2Z(TRI_UNKNOWN_MEM_ZONE, p, length);
  }

  if (copy == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  try {
    _strings.push_back(copy);
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

char* Query::registerString (std::string const& p,
                             bool mustUnescape) {

  return registerString(p.c_str(), p.length(), mustUnescape);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief enter a V8 context
////////////////////////////////////////////////////////////////////////////////

void Query::enterContext () {
  if (! _contextOwnedByExterior) {
    if (_context == nullptr) {
      _context = _applicationV8->enterContext("STANDARD", _vocbase, false, false);

      if (_context == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "cannot enter V8 context");
      }
    
      // register transaction and resolver in context
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
  else {
    return ExecutionStats::toJsonStatic();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fetch a boolean value from the options
////////////////////////////////////////////////////////////////////////////////

bool Query::getBooleanOption (char const* option, bool defaultValue) const {  
  if (! TRI_IsArrayJson(_options)) {
    return defaultValue;
  }

  TRI_json_t const* valueJson = TRI_LookupArrayJson(_options, option);
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
  TRI_json_t* json = TRI_CreateList2Json(zone, n);

  if (json != nullptr) {
    for (size_t i = 0; i < n; ++i) {
      TRI_json_t* error = TRI_CreateArray2Json(zone, 2);

      if (error != nullptr) {
        TRI_Insert3ArrayJson(zone, error, "code", TRI_CreateNumberJson(zone, static_cast<double>(_warnings[i].first)));
        TRI_Insert3ArrayJson(zone, error, "message", TRI_CreateString2CopyJson(zone, _warnings[i].second.c_str(), _warnings[i].second.size()));

        TRI_PushBack3ListJson(zone, json, error);
      }
    }
  }

  return json;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief fetch a numeric value from the options
////////////////////////////////////////////////////////////////////////////////

double Query::getNumericOption (char const* option, double defaultValue) const {  
  if (! TRI_IsArrayJson(_options)) {
    return defaultValue;
  }

  TRI_json_t const* valueJson = TRI_LookupArrayJson(_options, option);
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
  if (! TRI_IsArrayJson(_options)) {
    return true; // default
  }
  
  TRI_json_t const* optJson = TRI_LookupArrayJson(_options, "optimizer");

  if (! TRI_IsArrayJson(optJson)) {
    return true; // default
  }
  
  TRI_json_t const* j = TRI_LookupArrayJson(optJson, "inspectSimplePlans");
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

  if (! TRI_IsArrayJson(_options)) {
    return rules;
  }
  
  TRI_json_t const* optJson = TRI_LookupArrayJson(_options, "optimizer");

  if (! TRI_IsArrayJson(optJson)) {
    return rules;
  }
  
  TRI_json_t const* rulesJson = TRI_LookupArrayJson(optJson, "rules");

  if (! TRI_IsListJson(rulesJson)) {
    return rules;
  }
        
  size_t const n = TRI_LengthListJson(rulesJson);

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
    _profile->enter(_state);
  }

#if 0
  // Just for debugging:
  std::cout << "enterState: " << state;
  if (_queryString != nullptr) {
    std::cout << _queryString << std::endl;
  }
  else {
    std::cout << "no querystring" << std::endl;
  }
#endif

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

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
