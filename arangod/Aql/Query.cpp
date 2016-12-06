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

#include "Query.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Executor.h"
#include "Aql/Optimizer.h"
#include "Aql/Parser.h"
#include "Aql/QueryCache.h"
#include "Aql/QueryList.h"
#include "Basics/Exceptions.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WorkMonitor.h"
#include "Basics/fasthash.h"
#include "Cluster/ServerState.h"
#include "Logger/Logger.h"
#include "Utils/Transaction.h"
#include "Utils/AqlTransaction.h"
#include "Utils/StandaloneTransactionContext.h"
#include "Utils/V8TransactionContext.h"
#include "V8/v8-conv.h"
#include "V8/v8-vpack.h"
#include "V8Server/V8DealerFeature.h"
#include "VocBase/Graphs.h"
#include "VocBase/vocbase.h"

using namespace arangodb;
using namespace arangodb::aql;

namespace {
static std::atomic<TRI_voc_tick_t> NextQueryId(1);
}

/// @brief names of query phases / states
static std::string StateNames[] = {
    "initializing",        // INITIALIZATION
    "parsing",             // PARSING
    "optimizing ast",      // AST_OPTIMIZATION
    "loading collections", // LOADING_COLLECTIONS
    "instantiating plan",  // PLAN_INSTANTIATION
    "optimizing plan",     // PLAN_OPTIMIZATION
    "executing",           // EXECUTION
    "finalizing",          // FINALIZATION

    "invalid"  // INVALID
};

// make sure the state strings and the actual states match
static_assert(sizeof(StateNames) / sizeof(std::string) ==
                  static_cast<size_t>(ExecutionState::INVALID_STATE) + 1,
              "invalid number of ExecutionState values");

/// @brief create a profile
Profile::Profile(Query* query)
    : query(query), results(), stamp(query->startTime()), tracked(false) {
  auto queryList = query->vocbase()->queryList();

  try {
    tracked = queryList->insert(query, stamp);
  } catch (...) {
  }
}

/// @brief destroy a profile
Profile::~Profile() {
  // only remove from list when the query was inserted into it...
  if (tracked) {
    auto queryList = query->vocbase()->queryList();

    try {
      queryList->remove(query, stamp);
     } catch (...) {
    }
  }
}

/// @brief sets a state to done
void Profile::setDone(ExecutionState state) {
  double const now = TRI_microtime();

  if (state != ExecutionState::INVALID_STATE) {
    // record duration of state
    results.emplace_back(state, now - stamp);
  }

  // set timestamp
  stamp = now;
}

/// @brief convert the profile to VelocyPack
std::shared_ptr<VPackBuilder> Profile::toVelocyPack() {
  auto result = std::make_shared<VPackBuilder>();
  {
    VPackObjectBuilder b(result.get());
    for (auto const& it : results) {
      result->add(StateNames[static_cast<int>(it.first)],
                  VPackValue(it.second));
    }
  }
  return result;
}

/// @brief global threshold value for slow queries
double Query::SlowQueryThresholdValue = 10.0;

/// @brief whether or not query tracking is disabled globally
bool Query::DoDisableQueryTracking = false;

/// @brief creates a query
Query::Query(bool contextOwnedByExterior, TRI_vocbase_t* vocbase,
             char const* queryString, size_t queryLength,
             std::shared_ptr<VPackBuilder> bindParameters,
             std::shared_ptr<VPackBuilder> options, QueryPart part)
    : _id(0),
      _resourceMonitor(),
      _resources(&_resourceMonitor),
      _vocbase(vocbase),
      _executor(nullptr),
      _context(nullptr),
      _queryString(queryString),
      _queryLength(queryLength),
      _queryBuilder(),
      _bindParameters(bindParameters),
      _options(options),
      _collections(vocbase),
      _ast(nullptr),
      _profile(nullptr),
      _state(INVALID_STATE),
      _plan(nullptr),
      _parser(nullptr),
      _trx(nullptr),
      _engine(nullptr),
      _maxWarningCount(10),
      _warnings(),
      _startTime(TRI_microtime()),
      _part(part),
      _contextOwnedByExterior(contextOwnedByExterior),
      _killed(false),
      _isModificationQuery(false) {
  // std::cout << TRI_CurrentThreadId() << ", QUERY " << this << " CTOR: " <<
  // queryString << "\n";
  double tracing = getNumericOption("tracing", 0);
  if (tracing > 0) {
    LOG_TOPIC(INFO, Logger::QUERIES)
      << TRI_microtime() - _startTime << " "
      << "Query::Query queryString: " << std::string(queryString, queryLength)
      << " this: " << (uintptr_t) this;
  } else {
    LOG_TOPIC(DEBUG, Logger::QUERIES)
      << TRI_microtime() - _startTime << " "
      << "Query::Query queryString: " << std::string(queryString, queryLength)
      << " this: " << (uintptr_t) this;
  }
  if (bindParameters != nullptr && !bindParameters->isEmpty() &&
      !bindParameters->slice().isNone()) {
    if (tracing > 0) {
      LOG_TOPIC(INFO, Logger::QUERIES)
          << "bindParameters: " << bindParameters->slice().toJson();
    } else {
      LOG_TOPIC(DEBUG, Logger::QUERIES)
          << "bindParameters: " << bindParameters->slice().toJson();
    }
  }
  if (options != nullptr && !options->isEmpty() && !options->slice().isNone()) {
    if (tracing > 0) {
      LOG_TOPIC(INFO, Logger::QUERIES)
          << "options: " << options->slice().toJson();
    } else {
      LOG_TOPIC(DEBUG, Logger::QUERIES)
          << "options: " << options->slice().toJson();
    }
  }
  TRI_ASSERT(_vocbase != nullptr);
        
  _resourceMonitor.setMemoryLimit(memoryLimit());
}

/// @brief creates a query from VelocyPack
Query::Query(bool contextOwnedByExterior, TRI_vocbase_t* vocbase,
             std::shared_ptr<VPackBuilder> const queryStruct,
             std::shared_ptr<VPackBuilder> options, QueryPart part)
    : _id(0),
      _resourceMonitor(),
      _resources(&_resourceMonitor),
      _vocbase(vocbase),
      _executor(nullptr),
      _context(nullptr),
      _queryString(nullptr),
      _queryLength(0),
      _queryBuilder(queryStruct),
      _bindParameters(nullptr),
      _options(options),
      _collections(vocbase),
      _ast(nullptr),
      _profile(nullptr),
      _state(INVALID_STATE),
      _plan(nullptr),
      _parser(nullptr),
      _trx(nullptr),
      _engine(nullptr),
      _maxWarningCount(10),
      _warnings(),
      _startTime(TRI_microtime()),
      _part(part),
      _contextOwnedByExterior(contextOwnedByExterior),
      _killed(false),
      _isModificationQuery(false) {

  LOG_TOPIC(DEBUG, Logger::QUERIES)
      << TRI_microtime() - _startTime << " "
      << "Query::Query queryStruct: " << queryStruct->slice().toJson()
      << " this: " << (uintptr_t) this;
  if (options != nullptr && !options->isEmpty() && !options->slice().isNone()) {
    LOG_TOPIC(DEBUG, Logger::QUERIES)
        << "options: " << options->slice().toJson();
  }
  TRI_ASSERT(_vocbase != nullptr);
  
  _resourceMonitor.setMemoryLimit(memoryLimit());
}

/// @brief destroys a query
Query::~Query() {
  double tracing = getNumericOption("tracing", 0);
  if (tracing > 0) {
    LOG_TOPIC(INFO, Logger::QUERIES)
      << TRI_microtime() - _startTime << " "
      << "Query::~Query queryString: "
      << " this: " << (uintptr_t) this;
  }
  cleanupPlanAndEngine(TRI_ERROR_INTERNAL);  // abort the transaction

  delete _profile;
  _profile = nullptr;

  delete _executor;
  _executor = nullptr;

  if (_context != nullptr) {
    TRI_ASSERT(!_contextOwnedByExterior);

    // unregister transaction and resolver in context
    ISOLATE;
    TRI_GET_GLOBALS();
    auto ctx =
        static_cast<arangodb::V8TransactionContext*>(v8g->_transactionContext);
    if (ctx != nullptr) {
      ctx->unregisterTransaction();
    }

    V8DealerFeature::DEALER->exitContext(_context);
    _context = nullptr;
  }

  delete _ast;
  _ast = nullptr;

  for (auto& it : _graphs) {
    delete it.second;
  }
  LOG_TOPIC(DEBUG, Logger::QUERIES)
      << TRI_microtime() - _startTime << " "
      << "Query::~Query this: " << (uintptr_t) this;
}

/// @brief clone a query
/// note: as a side-effect, this will also create and start a transaction for
/// the query
Query* Query::clone(QueryPart part, bool withPlan) {
  auto clone =
      std::make_unique<Query>(false, _vocbase, _queryString, _queryLength,
                              std::shared_ptr<VPackBuilder>(), _options, part);

  clone->_resourceMonitor = _resourceMonitor;
  clone->_resourceMonitor.clear();

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

void Query::setExecutionTime() {
  if (_engine != nullptr) {
    _engine->_stats.setExecutionTime(TRI_microtime() - _startTime);
  }
}

/// @brief extract a region from the query
std::string Query::extractRegion(int line, int column) const {
  TRI_ASSERT(_queryString != nullptr);

  // note: line numbers reported by bison/flex start at 1, columns start at 0
  int currentLine = 1;
  int currentColumn = 0;

  char c;
  char const* p = _queryString;

  while ((static_cast<size_t>(p - _queryString) < _queryLength) && (c = *p)) {
    if (currentLine > line ||
        (currentLine >= line && currentColumn >= column)) {
      break;
    }

    if (c == '\n') {
      ++p;
      ++currentLine;
      currentColumn = 0;
    } else if (c == '\r') {
      ++p;
      ++currentLine;
      currentColumn = 0;

      // eat a following newline
      if (*p == '\n') {
        ++p;
      }
    } else {
      ++currentColumn;
      ++p;
    }
  }

  // p is pointing at the position in the query the parse error occurred at
  TRI_ASSERT(p >= _queryString);

  size_t offset = static_cast<size_t>(p - _queryString);

  static int const SNIPPET_LENGTH = 32;
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

/// @brief register an error, with an optional parameter inserted into printf
/// this also makes the query abort
void Query::registerError(int code, char const* details) {
  TRI_ASSERT(code != TRI_ERROR_NO_ERROR);

  if (details == nullptr) {
    THROW_ARANGO_EXCEPTION(code);
  }

  THROW_ARANGO_EXCEPTION_PARAMS(code, details);
}

/// @brief register an error with a custom error message
/// this also makes the query abort
void Query::registerErrorCustom(int code, char const* details) {
  TRI_ASSERT(code != TRI_ERROR_NO_ERROR);

  if (details == nullptr) {
    THROW_ARANGO_EXCEPTION(code);
  }

  std::string errorMessage(TRI_errno_string(code));
  errorMessage.append(": ");
  errorMessage.append(details);

  THROW_ARANGO_EXCEPTION_MESSAGE(code, errorMessage);
}

/// @brief register a warning
void Query::registerWarning(int code, char const* details) {
  TRI_ASSERT(code != TRI_ERROR_NO_ERROR);

  if (_warnings.size() > _maxWarningCount) {
    return;
  }

  if (details == nullptr) {
    _warnings.emplace_back(code, TRI_errno_string(code));
  } else {
    _warnings.emplace_back(code, details);
  }
}

/// @brief prepare an AQL query, this is a preparation for execute, but
/// execute calls it internally. The purpose of this separate method is
/// to be able to only prepare a query from VelocyPack and then store it in the
/// QueryRegistry.
QueryResult Query::prepare(QueryRegistry* registry) {
  LOG_TOPIC(DEBUG, Logger::QUERIES) << TRI_microtime() - _startTime << " "
                                    << "Query::prepare"
                                    << " this: " << (uintptr_t) this;
  TRI_ASSERT(registry != nullptr);

  try {
    init();
    enterState(PARSING);

    auto parser = std::make_unique<Parser>(this);
    std::unique_ptr<ExecutionPlan> plan;

    if (_queryString != nullptr) {
      parser->parse(false);
      // put in bind parameters
      parser->ast()->injectBindParameters(_bindParameters);
    }

    _isModificationQuery = parser->isModificationQuery();

    // create the transaction object, but do not start it yet
    arangodb::AqlTransaction* trx = new arangodb::AqlTransaction(
        createTransactionContext(), _collections.collections(),
        _part == PART_MAIN);
    _trx = trx;

    try {
      bool planRegisters;
      // As soon as we start du instantiate the plan we have to clean it
      // up before killing the unique_ptr
      if (_queryString != nullptr) {
        // we have an AST
        // optimize the ast
        enterState(AST_OPTIMIZATION);

        parser->ast()->validateAndOptimize();
        
        enterState(LOADING_COLLECTIONS);
      
        int res = trx->begin();

        if (res != TRI_ERROR_NO_ERROR) {
          return transactionError(res);
        }

        enterState(PLAN_INSTANTIATION);
        plan.reset(ExecutionPlan::instantiateFromAst(parser->ast()));

        if (plan.get() == nullptr) {
          // oops
          return QueryResult(TRI_ERROR_INTERNAL,
                             "failed to create query execution engine");
        }

        // Run the query optimizer:
        enterState(PLAN_OPTIMIZATION);
        arangodb::aql::Optimizer opt(maxNumberOfPlans());
        // get enabled/disabled rules
        opt.createPlans(plan.release(), getRulesFromOptions(),
                        inspectSimplePlans());
        // Now plan and all derived plans belong to the optimizer
        plan.reset(opt.stealBest());  // Now we own the best one again
        planRegisters = true;
      } else {  // no queryString, we are instantiating from _queryBuilder
        enterState(PARSING);

        VPackSlice const querySlice = _queryBuilder->slice();
        ExecutionPlan::getCollectionsFromVelocyPack(parser->ast(), querySlice);

        parser->ast()->variables()->fromVelocyPack(querySlice);
        // creating the plan may have produced some collections
        // we need to add them to the transaction now (otherwise the query will
        // fail)
        
        enterState(LOADING_COLLECTIONS);
        
        int res = trx->addCollectionList(_collections.collections());

        if (res == TRI_ERROR_NO_ERROR) {
          res = trx->begin();
        }

        if (res != TRI_ERROR_NO_ERROR) {
          return transactionError(res);
        }
        
        enterState(PLAN_INSTANTIATION);

        // we have an execution plan in VelocyPack format
        plan.reset(ExecutionPlan::instantiateFromVelocyPack(
            parser->ast(), _queryBuilder->slice()));
        if (plan.get() == nullptr) {
          // oops
          return QueryResult(TRI_ERROR_INTERNAL);
        }

        planRegisters = false;
      }

      TRI_ASSERT(plan.get() != nullptr);

      // varsUsedLater and varsValid are unordered_sets and so their orders
      // are not the same in the serialized and deserialized plans

      // return the V8 context
      exitContext();

      enterState(EXECUTION);
      ExecutionEngine* engine(ExecutionEngine::instantiateFromPlan(
          registry, this, plan.get(), planRegisters));

      // If all went well so far, then we keep _plan, _parser and _trx and
      // return:
      _plan = plan.release();
      _parser = parser.release();
      _engine = engine;
      return QueryResult();
    } catch (arangodb::basics::Exception const& ex) {
      cleanupPlanAndEngine(ex.code());
      return QueryResult(ex.code(), ex.message() + getStateString());
    } catch (std::bad_alloc const&) {
      cleanupPlanAndEngine(TRI_ERROR_OUT_OF_MEMORY);
      return QueryResult(
          TRI_ERROR_OUT_OF_MEMORY,
          TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY) + getStateString());
    } catch (std::exception const& ex) {
      cleanupPlanAndEngine(TRI_ERROR_INTERNAL);
      return QueryResult(TRI_ERROR_INTERNAL, ex.what() + getStateString());
    } catch (...) {
      cleanupPlanAndEngine(TRI_ERROR_INTERNAL);
      return QueryResult(TRI_ERROR_INTERNAL,
                         TRI_errno_string(TRI_ERROR_INTERNAL) + getStateString());
    }
  } catch (arangodb::basics::Exception const& ex) {
    return QueryResult(ex.code(), ex.message() + getStateString());
  } catch (std::bad_alloc const&) {
    return QueryResult(
        TRI_ERROR_OUT_OF_MEMORY,
        TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY) + getStateString());
  } catch (std::exception const& ex) {
    return QueryResult(TRI_ERROR_INTERNAL, ex.what() + getStateString());
  } catch (...) {
    return QueryResult(TRI_ERROR_INTERNAL,
                       TRI_errno_string(TRI_ERROR_INTERNAL) + getStateString());
  }

}

/// @brief execute an AQL query
QueryResult Query::execute(QueryRegistry* registry) {
  LOG_TOPIC(DEBUG, Logger::QUERIES) << TRI_microtime() - _startTime << " "
                                    << "Query::execute"
                                    << " this: " << (uintptr_t) this;
  TRI_ASSERT(registry != nullptr);

  std::unique_ptr<AqlWorkStack> work;

  try {
    bool useQueryCache = canUseQueryCache();
    uint64_t queryStringHash = 0;

    if (useQueryCache) {
      // hash the query
      queryStringHash = hash();

      // check the query cache for an existing result
      auto cacheEntry = arangodb::aql::QueryCache::instance()->lookup(
          _vocbase, queryStringHash, _queryString, _queryLength);
      arangodb::aql::QueryCacheResultEntryGuard guard(cacheEntry);

      if (cacheEntry != nullptr) {
        // got a result from the query cache
        QueryResult res(TRI_ERROR_NO_ERROR);
        // we don't have yet a transaction when we're here, so let's create
        // a mimimal context to build the result
        res.context = std::make_shared<StandaloneTransactionContext>(_vocbase);

        res.warnings = warningsToVelocyPack();
        TRI_ASSERT(cacheEntry->_queryResult != nullptr);
        res.result = cacheEntry->_queryResult;
        res.cached = true;
        return res;
      }
    }

    QueryResult result = prepare(registry);

    if (result.code != TRI_ERROR_NO_ERROR) {
      return result;
    }

    if (_queryString == nullptr) {
      // we don't have query string... now pass query id to WorkMonitor
      work.reset(new AqlWorkStack(_vocbase, _id));
    } else {
      // we do have a query string... pass query to WorkMonitor
      work.reset(new AqlWorkStack(_vocbase, _id, _queryString, _queryLength));
    }

    log();

    if (useQueryCache && (_isModificationQuery || !_warnings.empty() ||
                          !_ast->root()->isCacheable())) {
      useQueryCache = false;
    }

    AqlItemBlock* value = nullptr;
    VPackOptions options = VPackOptions::Defaults;
    options.buildUnindexedArrays = true;
    options.buildUnindexedObjects = true;

    TRI_ASSERT(_engine != nullptr);
    auto resultBuilder = std::make_shared<VPackBuilder>(&options);
    resultBuilder->buffer()->reserve(
        16 * 1024);  // reserve some space in Builder to avoid frequent reallocs

    try {
      resultBuilder->openArray();
      // this is the RegisterId our results can be found in
      auto const resultRegister = _engine->resultRegister();

      if (useQueryCache) {
        // iterate over result, return it and store it in query cache
        while (nullptr != (value = _engine->getSome(
                               1, ExecutionBlock::DefaultBatchSize()))) {
          size_t const n = value->size();

          for (size_t i = 0; i < n; ++i) {
            AqlValue const& val = value->getValueReference(i, resultRegister);

            if (!val.isEmpty()) {
              val.toVelocyPack(_trx, *resultBuilder, true);
            }
          }
          delete value;
          value = nullptr;
        }

        // must close result array here because it must be passed as a closed
        // array
        // to the query cache
        resultBuilder->close();

        if (_warnings.empty()) {
          // finally store the generated result in the query cache
          auto result = QueryCache::instance()->store(
              _vocbase, queryStringHash, _queryString, _queryLength,
              resultBuilder, _trx->collectionNames());

          if (result == nullptr) {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
          }
        }
      } else {
        // iterate over result and return it
        while (nullptr != (value = _engine->getSome(
                               1, ExecutionBlock::DefaultBatchSize()))) {
          size_t const n = value->size();
          for (size_t i = 0; i < n; ++i) {
            AqlValue const& val = value->getValueReference(i, resultRegister);

            if (!val.isEmpty()) {
              val.toVelocyPack(_trx, *resultBuilder, false);
            }
          }
          delete value;
          value = nullptr;
        }

        // must close result array
        resultBuilder->close();
      }
    } catch (...) {
      delete value;
      throw;
    }

    _trx->commit();

    _engine->_stats.setExecutionTime(TRI_microtime() - _startTime);
    auto stats = std::make_shared<VPackBuilder>();
    _engine->_stats.toVelocyPack(*(stats.get()));

    result.context = _trx->transactionContext();

    cleanupPlanAndEngine(TRI_ERROR_NO_ERROR);

    enterState(FINALIZATION);

    result.warnings = warningsToVelocyPack();
    result.result = resultBuilder;
    result.stats = stats;

    if (_profile != nullptr && profiling()) {
      result.profile = _profile->toVelocyPack();
    }

    // patch stats in place
    // we do this because "executionTime" should include the whole span of the execution and we have to set it at the very end
    basics::VelocyPackHelper::patchDouble(result.stats->slice().get("executionTime"), TRI_microtime() - _startTime);

    LOG_TOPIC(DEBUG, Logger::QUERIES) << TRI_microtime() - _startTime << " "
                                      << "Query::execute:returning"
                                      << " this: " << (uintptr_t) this;
    
    return result;
  } catch (arangodb::basics::Exception const& ex) {
    setExecutionTime();
    cleanupPlanAndEngine(ex.code());
    return QueryResult(ex.code(), ex.message() + getStateString());
  } catch (std::bad_alloc const&) {
    setExecutionTime();
    cleanupPlanAndEngine(TRI_ERROR_OUT_OF_MEMORY);
    return QueryResult(
        TRI_ERROR_OUT_OF_MEMORY,
        TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY) + getStateString());
  } catch (std::exception const& ex) {
    setExecutionTime();
    cleanupPlanAndEngine(TRI_ERROR_INTERNAL);
    return QueryResult(TRI_ERROR_INTERNAL, ex.what() + getStateString());
  } catch (...) {
    setExecutionTime();
    cleanupPlanAndEngine(TRI_ERROR_INTERNAL);
    return QueryResult(TRI_ERROR_INTERNAL,
                       TRI_errno_string(TRI_ERROR_INTERNAL) + getStateString());
  }
}

// execute an AQL query: may only be called with an active V8 handle scope
QueryResultV8 Query::executeV8(v8::Isolate* isolate, QueryRegistry* registry) {
  LOG_TOPIC(DEBUG, Logger::QUERIES) << TRI_microtime() - _startTime << " "
                                    << "Query::executeV8"
                                    << " this: " << (uintptr_t) this;
  std::unique_ptr<AqlWorkStack> work;

  try {
    bool useQueryCache = canUseQueryCache();
    uint64_t queryStringHash = 0;

    if (useQueryCache) {
      // hash the query
      queryStringHash = hash();

      // check the query cache for an existing result
      auto cacheEntry = arangodb::aql::QueryCache::instance()->lookup(
          _vocbase, queryStringHash, _queryString, _queryLength);
      arangodb::aql::QueryCacheResultEntryGuard guard(cacheEntry);

      if (cacheEntry != nullptr) {
        // got a result from the query cache
        QueryResultV8 res(TRI_ERROR_NO_ERROR);
        // we don't have yet a transaction when we're here, so let's create
        // a mimimal context to build the result
        res.context = std::make_shared<StandaloneTransactionContext>(_vocbase);

        v8::Handle<v8::Value> values =
            TRI_VPackToV8(isolate, cacheEntry->_queryResult->slice(),
                          res.context->getVPackOptions());
        TRI_ASSERT(values->IsArray());
        res.result = v8::Handle<v8::Array>::Cast(values);
        res.cached = true;
        return res;
      }
    }

    QueryResultV8 result = prepare(registry);

    if (result.code != TRI_ERROR_NO_ERROR) {
      return result;
    }

    work.reset(new AqlWorkStack(_vocbase, _id, _queryString, _queryLength));

    log();

    if (useQueryCache && (_isModificationQuery || !_warnings.empty() ||
                          !_ast->root()->isCacheable())) {
      useQueryCache = false;
    }

    result.result = v8::Array::New(isolate);

    TRI_ASSERT(_engine != nullptr);

    // this is the RegisterId our results can be found in
    auto const resultRegister = _engine->resultRegister();
    AqlItemBlock* value = nullptr;

    try {
      if (useQueryCache) {
        VPackOptions options = VPackOptions::Defaults;
        options.buildUnindexedArrays = true;
        options.buildUnindexedObjects = true;

        // iterate over result, return it and store it in query cache
        auto builder = std::make_shared<VPackBuilder>(&options);
        builder->openArray();

        uint32_t j = 0;
        while (nullptr != (value = _engine->getSome(
                               1, ExecutionBlock::DefaultBatchSize()))) {
          size_t const n = value->size();

          for (size_t i = 0; i < n; ++i) {
            AqlValue const& val = value->getValueReference(i, resultRegister);

            if (!val.isEmpty()) {
              result.result->Set(j++, val.toV8(isolate, _trx));
              val.toVelocyPack(_trx, *builder, true);
            }
          }
          delete value;
          value = nullptr;
        }

        builder->close();

        if (_warnings.empty()) {
          // finally store the generated result in the query cache
          QueryCache::instance()->store(_vocbase, queryStringHash, _queryString,
                                        _queryLength, builder,
                                        _trx->collectionNames());
        }
      } else {
        // iterate over result and return it
        bool suppressResult = silent();

        uint32_t j = 0;
        while (nullptr != (value = _engine->getSome(
                               1, ExecutionBlock::DefaultBatchSize()))) {
          if (!suppressResult) {
            size_t const n = value->size();

            for (size_t i = 0; i < n; ++i) {
              AqlValue const& val = value->getValueReference(i, resultRegister);

              if (!val.isEmpty()) {
                result.result->Set(j++, val.toV8(isolate, _trx));
              }

              if (V8PlatformFeature::isOutOfMemory(isolate)) {
                THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
              }
            }
          }
          delete value;
          value = nullptr;
        }
      }
    } catch (...) {
      delete value;
      throw;
    }

    LOG_TOPIC(DEBUG, Logger::QUERIES) << TRI_microtime() - _startTime << " "
                                      << "Query::executeV8: before _trx->commit"
                                      << " this: " << (uintptr_t) this;

    _trx->commit();

    _engine->_stats.setExecutionTime(TRI_microtime() - _startTime);
    auto stats = std::make_shared<VPackBuilder>();
    _engine->_stats.toVelocyPack(*(stats.get()));

    result.context = _trx->transactionContext();

    LOG_TOPIC(DEBUG, Logger::QUERIES)
        << TRI_microtime() - _startTime << " "
        << "Query::executeV8: before cleanupPlanAndEngine"
        << " this: " << (uintptr_t) this;

    cleanupPlanAndEngine(TRI_ERROR_NO_ERROR);

    enterState(FINALIZATION);

    result.warnings = warningsToVelocyPack();
    result.stats = stats;

    if (_profile != nullptr && profiling()) {
      result.profile = _profile->toVelocyPack();
    }
    
    // patch executionTime stats value in place
    // we do this because "executionTime" should include the whole span of the execution and we have to set it at the very end
    basics::VelocyPackHelper::patchDouble(result.stats->slice().get("executionTime"), TRI_microtime() - _startTime);

    return result;
  } catch (arangodb::basics::Exception const& ex) {
    setExecutionTime();
    cleanupPlanAndEngine(ex.code());
    return QueryResultV8(ex.code(), ex.message() + getStateString());
  } catch (std::bad_alloc const&) {
    setExecutionTime();
    cleanupPlanAndEngine(TRI_ERROR_OUT_OF_MEMORY);
    return QueryResultV8(
        TRI_ERROR_OUT_OF_MEMORY,
        TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY) + getStateString());
  } catch (std::exception const& ex) {
    setExecutionTime();
    cleanupPlanAndEngine(TRI_ERROR_INTERNAL);
    return QueryResultV8(TRI_ERROR_INTERNAL, ex.what() + getStateString());
  } catch (...) {
    setExecutionTime();
    cleanupPlanAndEngine(TRI_ERROR_INTERNAL);
    return QueryResult(TRI_ERROR_INTERNAL,
                       TRI_errno_string(TRI_ERROR_INTERNAL) + getStateString());
  }
}

/// @brief parse an AQL query
QueryResult Query::parse() {
  try {
    init();
    Parser parser(this);
    return parser.parse(true);
  } catch (arangodb::basics::Exception const& ex) {
    return QueryResult(ex.code(), ex.message());
  } catch (std::bad_alloc const&) {
    cleanupPlanAndEngine(TRI_ERROR_OUT_OF_MEMORY);
    return QueryResult(TRI_ERROR_OUT_OF_MEMORY,
                       TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY));
  } catch (std::exception const& ex) {
    cleanupPlanAndEngine(TRI_ERROR_INTERNAL);
    return QueryResult(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    return QueryResult(TRI_ERROR_OUT_OF_MEMORY,
                       TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY));
  }
}

/// @brief explain an AQL query
QueryResult Query::explain() {
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
    
    enterState(LOADING_COLLECTIONS);

    // create the transaction object, but do not start it yet
    _trx = new arangodb::AqlTransaction(createTransactionContext(),
                                        _collections.collections(), true);

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
    arangodb::aql::Optimizer opt(maxNumberOfPlans());
    // get enabled/disabled rules
    opt.createPlans(plan, getRulesFromOptions(), inspectSimplePlans());

    enterState(FINALIZATION);

    QueryResult result(TRI_ERROR_NO_ERROR);

    if (allPlans()) {
      result.result = std::make_shared<VPackBuilder>();
      {
        VPackArrayBuilder guard(result.result.get());

        auto plans = opt.getPlans();
        for (auto& it : plans) {
          TRI_ASSERT(it != nullptr);

          it->findVarUsage();
          it->planRegisters();
          it->toVelocyPack(*result.result.get(), parser.ast(), verbosePlans());
        }
      }
      // cacheability not available here
      result.cached = false;
    } else {
      // Now plan and all derived plans belong to the optimizer
      std::unique_ptr<ExecutionPlan> bestPlan(
          opt.stealBest());  // Now we own the best one again
      TRI_ASSERT(bestPlan != nullptr);

      bestPlan->findVarUsage();
      bestPlan->planRegisters();

      result.result = bestPlan->toVelocyPack(parser.ast(), verbosePlans());

      // cacheability
      result.cached = (_queryString != nullptr && _queryLength > 0 &&
                       !_isModificationQuery && _warnings.empty() &&
                       _ast->root()->isCacheable());
    }

    _trx->commit();

    result.warnings = warningsToVelocyPack();

    result.stats = opt._stats.toVelocyPack();

    return result;
  } catch (arangodb::basics::Exception const& ex) {
    return QueryResult(ex.code(), ex.message() + getStateString());
  } catch (std::bad_alloc const&) {
    return QueryResult(
        TRI_ERROR_OUT_OF_MEMORY,
        TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY) + getStateString());
  } catch (std::exception const& ex) {
    return QueryResult(TRI_ERROR_INTERNAL, ex.what() + getStateString());
  } catch (...) {
    return QueryResult(TRI_ERROR_INTERNAL,
                       TRI_errno_string(TRI_ERROR_INTERNAL) + getStateString());
  }
}

/// @brief get v8 executor
Executor* Query::executor() {
  if (_executor == nullptr) {
    // the executor is a singleton per query
    _executor = new Executor(literalSizeThreshold());
  }

  TRI_ASSERT(_executor != nullptr);
  return _executor;
}

/// @brief enter a V8 context
void Query::enterContext() {
  if (!_contextOwnedByExterior) {
    if (_context == nullptr) {
      _context = V8DealerFeature::DEALER->enterContext(_vocbase, false);

      if (_context == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                       "cannot enter V8 context");
      }

      // register transaction and resolver in context
      TRI_ASSERT(_trx != nullptr);

      ISOLATE;
      TRI_GET_GLOBALS();
      auto ctx = static_cast<arangodb::V8TransactionContext*>(
          v8g->_transactionContext);
      if (ctx != nullptr) {
        ctx->registerTransaction(_trx->getInternals());
      }
    }

    TRI_ASSERT(_context != nullptr);
  }
}

/// @brief return a V8 context
void Query::exitContext() {
  if (!_contextOwnedByExterior) {
    if (_context != nullptr) {
      // unregister transaction and resolver in context
      ISOLATE;
      TRI_GET_GLOBALS();
      auto ctx = static_cast<arangodb::V8TransactionContext*>(
          v8g->_transactionContext);
      if (ctx != nullptr) {
        ctx->unregisterTransaction();
      }

      V8DealerFeature::DEALER->exitContext(_context);
      _context = nullptr;
    }
    TRI_ASSERT(_context == nullptr);
  }
}

/// @brief returns statistics for current query.
void Query::getStats(VPackBuilder& builder) {
  if (_engine != nullptr) {
    _engine->_stats.setExecutionTime(TRI_microtime() - _startTime);
    _engine->_stats.toVelocyPack(builder);
  } else {
    ExecutionStats::toVelocyPackStatic(builder);
  }
}

/// @brief fetch a boolean value from the options
bool Query::getBooleanOption(char const* option, bool defaultValue) const {
  if (_options == nullptr) {
    return defaultValue;
  }

  VPackSlice options = _options->slice();
  if (!options.isObject()) {
    return defaultValue;
  }

  VPackSlice value = options.get(option);
  if (!value.isBoolean()) {
    return defaultValue;
  }

  return value.getBool();
}

/// @brief convert the list of warnings to VelocyPack.
///        Will add a new entry { ..., warnings: <warnings>, } if there are
///        warnings. If there are none it will not modify the builder
void Query::addWarningsToVelocyPackObject(
    arangodb::velocypack::Builder& builder) const {
  TRI_ASSERT(builder.isOpenObject());
  if (_warnings.empty()) {
    return;
  }
  size_t const n = _warnings.size();
  builder.add(VPackValue("warnings"));
  {
    VPackArrayBuilder guard(&builder);
    for (size_t i = 0; i < n; ++i) {
      VPackObjectBuilder objGuard(&builder);
      builder.add("code", VPackValue(_warnings[i].first));
      builder.add("message", VPackValue(_warnings[i].second));
    }
  }
}

/// @brief transform the list of warnings to VelocyPack.
std::shared_ptr<VPackBuilder> Query::warningsToVelocyPack() const {
  if (_warnings.empty()) {
    return nullptr;
  }
  auto result = std::make_shared<VPackBuilder>();
  {
    VPackArrayBuilder guard(result.get());
    size_t const n = _warnings.size();
    for (size_t i = 0; i < n; ++i) {
      VPackObjectBuilder objGuard(result.get());
      result->add("code", VPackValue(_warnings[i].first));
      result->add("message", VPackValue(_warnings[i].second));
    }
  }
  return result;
}

/// @brief initializes the query
void Query::init() {
  if (_id != 0) {
    // already called
    return;
  }

  TRI_ASSERT(_id == 0);
  TRI_ASSERT(_ast == nullptr);

  _id = Query::NextId();

  TRI_ASSERT(_profile == nullptr);
  _profile = new Profile(this);
  enterState(INITIALIZATION);

  TRI_ASSERT(_ast == nullptr);
  _ast = new Ast(this);
}

/// @brief log a query
void Query::log() {
  if (_queryString != nullptr) {
    static size_t const MaxLength = 1024;

    LOG_TOPIC(TRACE, Logger::QUERIES)
        << "executing query " << _id << ": '"
        << std::string(_queryString, (std::min)(_queryLength, MaxLength))
               .append(_queryLength > MaxLength ? "..." : "") << "'";
  }
}

/// @brief calculate a hash value for the query and bind parameters
uint64_t Query::hash() const {
  // hash the query string first
  uint64_t hash = arangodb::aql::QueryCache::instance()->hashQueryString(
      _queryString, _queryLength);

  // handle "fullCount" option. if this option is set, the query result will
  // be different to when it is not set!
  if (getBooleanOption("fullcount", false)) {
    hash = fasthash64(TRI_CHAR_LENGTH_PAIR("fullcount:true"), hash);
  } else {
    hash = fasthash64(TRI_CHAR_LENGTH_PAIR("fullcount:false"), hash);
  }

  // handle "count" option
  if (getBooleanOption("count", false)) {
    hash = fasthash64(TRI_CHAR_LENGTH_PAIR("count:true"), hash);
  } else {
    hash = fasthash64(TRI_CHAR_LENGTH_PAIR("count:false"), hash);
  }

  // blend query hash with bind parameters
  return hash ^ _bindParameters.hash();
}

/// @brief whether or not the query cache can be used for the query
bool Query::canUseQueryCache() const {
  if (_queryString == nullptr || _queryLength < 8) {
    return false;
  }

  auto queryCacheMode = QueryCache::instance()->mode();

  if (queryCacheMode == CACHE_ALWAYS_ON && getBooleanOption("cache", true)) {
    // cache mode is set to always on... query can still be excluded from cache
    // by
    // setting `cache` attribute to false.

    // cannot use query cache on a coordinator at the moment
    return !arangodb::ServerState::instance()->isRunningInCluster();
  } else if (queryCacheMode == CACHE_ON_DEMAND &&
             getBooleanOption("cache", false)) {
    // cache mode is set to demand... query will only be cached if `cache`
    // attribute is set to false

    // cannot use query cache on a coordinator at the moment
    return !arangodb::ServerState::instance()->isRunningInCluster();
  }

  return false;
}

/// @brief fetch a numeric value from the options
double Query::getNumericOption(char const* option, double defaultValue) const {
  if (_options == nullptr) {
    return defaultValue;
  }

  VPackSlice options = _options->slice();
  if (!options.isObject()) {
    return defaultValue;
  }

  VPackSlice value = options.get(option);
  if (!value.isNumber()) {
    return defaultValue;
  }

  return value.getNumericValue<double>();
}

/// @brief neatly format transaction error to the user.
QueryResult Query::transactionError(int errorCode) const {
  std::string err(TRI_errno_string(errorCode));

  auto detail = _trx->getErrorData();
  if (detail.size() > 0) {
    err += std::string(" (") + detail + std::string(")");
  }

  if (_queryString != nullptr && verboseErrors()) {
    err +=
        std::string("\nwhile executing:\n") + _queryString + std::string("\n");
  }

  return QueryResult(errorCode, err);
}

/// @brief read the "optimizer.inspectSimplePlans" section from the options
bool Query::inspectSimplePlans() const {
  if (_options == nullptr) {
    return true;
  }

  VPackSlice options = _options->slice();
  if (!options.isObject()) {
    return true;  // default
  }

  VPackSlice opt = options.get("optimizer");

  if (!opt.isObject()) {
    return true;  // default
  }

  return arangodb::basics::VelocyPackHelper::getBooleanValue(
      opt, "inspectSimplePlans", true);
}

/// @brief read the "optimizer.rules" section from the options
std::vector<std::string> Query::getRulesFromOptions() const {
  std::vector<std::string> rules;

  if (_options == nullptr) {
    return rules;
  }

  VPackSlice options = _options->slice();

  if (!options.isObject()) {
    return rules;
  }

  VPackSlice opt = options.get("optimizer");

  if (!opt.isObject()) {
    return rules;
  }

  VPackSlice rulesList = opt.get("rules");

  if (!rulesList.isArray()) {
    return rules;
  }

  for (auto const& rule : VPackArrayIterator(rulesList)) {
    if (rule.isString()) {
      rules.emplace_back(rule.copyString());
    }
  }

  return rules;
}

/// @brief enter a new state
void Query::enterState(ExecutionState state) {
  LOG_TOPIC(DEBUG, Logger::QUERIES) << TRI_microtime() - _startTime << " "
                                    << "Query::enterState: " << state
                                    << " this: " << (uintptr_t) this;
  if (_profile != nullptr) {
    // record timing for previous state
    _profile->setDone(_state);
  }

  // and adjust the state
  _state = state;
}

/// @brief get a description of the query's current state
std::string Query::getStateString() const {
  return std::string(" (while " + StateNames[_state] + ")");
}

/// @brief cleanup plan and engine for current query
void Query::cleanupPlanAndEngine(int errorCode) {
  if (_engine != nullptr) {
    try {
      _engine->shutdown(errorCode);
    } catch (...) {
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

/// @brief set the plan for the query
void Query::setPlan(ExecutionPlan* plan) {
  if (_plan != nullptr) {
    delete _plan;
  }
  _plan = plan;
}

/// @brief create a TransactionContext
std::shared_ptr<arangodb::TransactionContext>
Query::createTransactionContext() {
  if (_contextOwnedByExterior) {
    // we can use v8
    return arangodb::V8TransactionContext::Create(_vocbase, true);
  }

  return arangodb::StandaloneTransactionContext::Create(_vocbase);
}

/// @brief look up a graph either from our cache list or from the  _graphs
///        collection
Graph const* Query::lookupGraphByName(std::string const& name) {
  auto it = _graphs.find(name);

  if (it != _graphs.end()) {
    return it->second;
  }

  std::unique_ptr<arangodb::aql::Graph> g(
      arangodb::lookupGraphByName(_vocbase, name));

  if (g == nullptr) {
    return nullptr;
  }

  _graphs.emplace(name, g.get());

  return g.release();
}

/// @brief returns the next query id
TRI_voc_tick_t Query::NextId() {
  return NextQueryId.fetch_add(1, std::memory_order_seq_cst);
}
