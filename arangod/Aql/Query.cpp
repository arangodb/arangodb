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

#include "Aql/AqlItemBlock.h"
#include "Aql/AqlTransaction.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Optimizer.h"
#include "Aql/Parser.h"
#include "Aql/PlanCache.h"
#include "Aql/QueryCache.h"
#include "Aql/QueryList.h"
#include "Aql/QueryProfile.h"
#include "Aql/QueryRegistry.h"
#include "Basics/Exceptions.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/fasthash.h"
#include "Cluster/ServerState.h"
#include "Graph/Graph.h"
#include "Graph/GraphManager.h"
#include "Logger/Logger.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Transaction/V8Context.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/ExecContext.h"
#include "V8/v8-conv.h"
#include "V8/v8-vpack.h"
#include "V8Server/V8DealerFeature.h"
#include "VocBase/vocbase.h"

#include <velocypack/Iterator.h>

#ifndef USE_PLAN_CACHE
#undef USE_PLAN_CACHE
#endif

using namespace arangodb;
using namespace arangodb::aql;

namespace {
static std::atomic<TRI_voc_tick_t> nextQueryId(1);
}

/// @brief creates a query
Query::Query(
    bool contextOwnedByExterior,
    TRI_vocbase_t& vocbase,
    QueryString const& queryString,
    std::shared_ptr<VPackBuilder> const& bindParameters,
    std::shared_ptr<VPackBuilder> const& options,
    QueryPart part
)
    : _id(0),
      _resourceMonitor(),
      _resources(&_resourceMonitor),
      _vocbase(vocbase),
      _context(nullptr),
      _queryString(queryString),
      _queryBuilder(),
      _bindParameters(bindParameters),
      _options(options),
      _collections(&vocbase),
      _state(QueryExecutionState::ValueType::INVALID_STATE),
      _trx(nullptr),
      _warnings(),
      _startTime(TRI_microtime()),
      _part(part),
      _contextOwnedByExterior(contextOwnedByExterior),
      _killed(false),
      _isModificationQuery(false),
      _preparedV8Context(false),
      _executionPhase(ExecutionPhase::INITIALIZE),
      _sharedState(std::make_shared<SharedQueryState>()) {
  if (_contextOwnedByExterior) {
    // copy transaction options from global state into our local query options
    TransactionState* state = transaction::V8Context::getParentState();
    if (state != nullptr) {
      _queryOptions.transactionOptions = state->options();
    }
  }

  // populate query options
  if (_options != nullptr) {
    _queryOptions.fromVelocyPack(_options->slice());
  }

  // std::cout << TRI_CurrentThreadId() << ", QUERY " << this << " CTOR: " <<
  // queryString << "\n";
  ProfileLevel level = _queryOptions.profile;
  if (level >= PROFILE_LEVEL_TRACE_1) {
    LOG_TOPIC(INFO, Logger::QUERIES)
      << TRI_microtime() - _startTime << " "
      << "Query::Query queryString: " << _queryString
      << " this: " << (uintptr_t) this;
  } else {
    LOG_TOPIC(DEBUG, Logger::QUERIES)
      << TRI_microtime() - _startTime << " "
      << "Query::Query queryString: " << _queryString
      << " this: " << (uintptr_t) this;
  }

  if (bindParameters != nullptr && !bindParameters->isEmpty() &&
      !bindParameters->slice().isNone()) {
    if (level >= PROFILE_LEVEL_TRACE_1) {
      LOG_TOPIC(INFO, Logger::QUERIES)
          << "bindParameters: " << bindParameters->slice().toJson();
    } else {
      LOG_TOPIC(DEBUG, Logger::QUERIES)
          << "bindParameters: " << bindParameters->slice().toJson();
    }
  }

  if (options != nullptr && !options->isEmpty() && !options->slice().isNone()) {
    if (level >= PROFILE_LEVEL_TRACE_1) {
      LOG_TOPIC(INFO, Logger::QUERIES)
          << "options: " << options->slice().toJson();
    } else {
      LOG_TOPIC(DEBUG, Logger::QUERIES)
          << "options: " << options->slice().toJson();
    }
  }

  _resourceMonitor.setMemoryLimit(_queryOptions.memoryLimit);

  if (!AqlFeature::lease()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
  }
}

/// @brief creates a query from VelocyPack
Query::Query(
    bool contextOwnedByExterior,
    TRI_vocbase_t& vocbase,
    std::shared_ptr<VPackBuilder> const& queryStruct,
    std::shared_ptr<VPackBuilder> const& options,
    QueryPart part
)
    : _id(0),
      _resourceMonitor(),
      _resources(&_resourceMonitor),
      _vocbase(vocbase),
      _context(nullptr),
      _queryString(),
      _queryBuilder(queryStruct),
      _options(options),
      _collections(&vocbase),
      _state(QueryExecutionState::ValueType::INVALID_STATE),
      _trx(nullptr),
      _warnings(),
      _startTime(TRI_microtime()),
      _part(part),
      _contextOwnedByExterior(contextOwnedByExterior),
      _killed(false),
      _isModificationQuery(false),
      _preparedV8Context(false),
      _executionPhase(ExecutionPhase::INITIALIZE),
      _sharedState(std::make_shared<SharedQueryState>()) {

  // populate query options
  if (_options != nullptr) {
    _queryOptions.fromVelocyPack(_options->slice());
  }

  LOG_TOPIC(DEBUG, Logger::QUERIES)
      << TRI_microtime() - _startTime << " "
      << "Query::Query queryStruct: " << queryStruct->slice().toJson()
      << " this: " << (uintptr_t) this;
  if (options != nullptr && !options->isEmpty() && !options->slice().isNone()) {
    LOG_TOPIC(DEBUG, Logger::QUERIES)
        << "options: " << options->slice().toJson();
  }

  // adjust the _isModificationQuery value from the slice we got
  auto s = _queryBuilder->slice().get("isModificationQuery");
  if (s.isBoolean()) {
    _isModificationQuery = s.getBoolean();
  }

  _resourceMonitor.setMemoryLimit(_queryOptions.memoryLimit);

  if (!AqlFeature::lease()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
  }
}

/// @brief destroys a query
Query::~Query() {
  if (_queryOptions.profile >= PROFILE_LEVEL_TRACE_1) {
    LOG_TOPIC(INFO, Logger::QUERIES)
      << TRI_microtime() - _startTime << " "
      << "Query::~Query queryString: "
      << " this: " << (uintptr_t) this;
  }

  cleanupPlanAndEngineSync(TRI_ERROR_INTERNAL);

  exitContext();

  _ast.reset();
  _graphs.clear();

  LOG_TOPIC(DEBUG, Logger::QUERIES)
      << TRI_microtime() - _startTime << " "
      << "Query::~Query this: " << (uintptr_t) this;
  AqlFeature::unlease();
}

/// @brief clone a query
/// note: as a side-effect, this will also create and start a transaction for
/// the query
Query* Query::clone(QueryPart part, bool withPlan) {
  auto clone =
      std::make_unique<Query>(false, _vocbase, _queryString,
                              std::shared_ptr<VPackBuilder>(), _options, part);

  clone->_resourceMonitor = _resourceMonitor;
  clone->_resourceMonitor.clear();

  if (_isModificationQuery) {
    clone->setIsModificationQuery();
  }

  if (_plan != nullptr) {
    if (withPlan) {
      // clone the existing plan
      clone->_plan.reset(_plan->clone(*clone));
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
    clone->_plan.reset(new ExecutionPlan(ast()));
  }

  TRI_ASSERT(clone->_trx == nullptr);

  // A daughter transaction which does not
  // actually lock the collections
  clone->_trx = _trx->clone(_queryOptions.transactionOptions);

  Result res = clone->_trx->begin();

  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  return clone.release();
}

/// @brief whether or not the query is killed
bool Query::killed() const {
  return _killed;
}

/// @brief set the query to killed
void Query::kill() {
  _killed = true;
}

void Query::setExecutionTime() {
  if (_engine != nullptr) {
    _engine->_stats.setExecutionTime(TRI_microtime() - _startTime);
  }
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

  if (_queryOptions.failOnWarning) {
    // make an error from each warning if requested
    if (details == nullptr) {
      THROW_ARANGO_EXCEPTION(code);
    }
    THROW_ARANGO_EXCEPTION_MESSAGE(code, details);
  }

  if (_warnings.size() >= _queryOptions.maxWarningCount) {
    return;
  }

  if (details == nullptr) {
    _warnings.emplace_back(code, TRI_errno_string(code));
  } else {
    _warnings.emplace_back(code, details);
  }
}

void Query::prepare(QueryRegistry* registry) {
  TRI_ASSERT(registry != nullptr);

  init();
  enterState(QueryExecutionState::ValueType::PARSING);

  std::unique_ptr<ExecutionPlan> plan;

#if USE_PLAN_CACHE
  if (!_queryString.empty() &&
      hashQuery() != DontCache &&
      _part == PART_MAIN) {
    // LOG_TOPIC(INFO, Logger::FIXME) << "trying to find query in execution plan cache: '" << _queryString << "', hash: " << hashQuery();

    // store & lookup velocypack plans!!
    std::shared_ptr<PlanCacheEntry> planCacheEntry = PlanCache::instance()->lookup(_vocbase, hashQuery(), queryString);
    if (planCacheEntry != nullptr) {
      // LOG_TOPIC(INFO, Logger::FIXME) << "query found in execution plan cache: '" << _queryString << "'";

      TRI_ASSERT(_trx == nullptr);
      TRI_ASSERT(_collections.empty());

      // create the transaction object, but do not start it yet
      _trx = AqlTransaction::create(
              createTransactionContext(), _collections.collections(),
              _queryOptions.transactionOptions,
              _part == PART_MAIN);

      VPackBuilder* builder = planCacheEntry->builder.get();
      VPackSlice slice = builder->slice();
      ExecutionPlan::getCollectionsFromVelocyPack(_ast.get(), slice);
      _ast->variables()->fromVelocyPack(slice);

      enterState(QueryExecutionState::ValueType::LOADING_COLLECTIONS);

      int res = trx->addCollections(*_collections.collections());

      if(!trx->transactionContextPtr()->getParentTransaction()) {
        trx->addHint(transaction::Hints::Hint::FROM_TOPLEVEL_AQL);
      }

      if (res == TRI_ERROR_NO_ERROR) {
        res = _trx->begin();
      }

      if (res != TRI_ERROR_NO_ERROR) {
        THROW_ARANGO_EXCEPTION_MESSAGE(res, buildErrorMessage(res));
      }

      enterState(QueryExecutionState::ValueType::PLAN_INSTANTIATION);

      plan.reset(ExecutionPlan::instantiateFromVelocyPack(_ast.get(), slice));

      TRI_ASSERT(plan != nullptr);
    }
  }
#endif

  if (plan == nullptr) {
    plan.reset(preparePlan());

    TRI_ASSERT(plan != nullptr);

#if USE_PLAN_CACHE
    if (!_queryString.empty() &&
        hashQuery() != DontCache &&
        _part == PART_MAIN &&
        _warnings.empty() &&
        _ast->root()->isCacheable()) {
      // LOG_TOPIC(INFO, Logger::FIXME) << "storing query in execution plan cache '" << _queryString << "', hash: " << hashQuery();
      PlanCache::instance()->store(_vocbase, hashQuery(), _queryString, plan.get());
    }
#endif
  }

  TRI_ASSERT(plan != nullptr);
  plan->findVarUsage();

  enterState(QueryExecutionState::ValueType::EXECUTION);

  TRI_ASSERT(_engine == nullptr);
  TRI_ASSERT(_trx != nullptr);
  // note that the engine returned here may already be present in our
  // own _engine attribute (the instanciation procedure may modify us
  // by calling our engine(ExecutionEngine*) function
  // this is confusing and should be fixed!
  std::unique_ptr<ExecutionEngine> engine(ExecutionEngine::instantiateFromPlan(registry, this, plan.get(), !_queryString.empty()));

  if (_engine == nullptr) {
    _engine = std::move(engine);
  } else {
    engine.release();
  }

  _plan = std::move(plan);
}

/// @brief prepare an AQL query, this is a preparation for execute, but
/// execute calls it internally. The purpose of this separate method is
/// to be able to only prepare a query from VelocyPack and then store it in the
/// QueryRegistry.
ExecutionPlan* Query::preparePlan() {
  LOG_TOPIC(DEBUG, Logger::QUERIES) << TRI_microtime() - _startTime << " "
                                    << "Query::prepare"
                                    << " this: " << (uintptr_t) this;

  auto ctx = createTransactionContext();
  std::unique_ptr<ExecutionPlan> plan;

  if (!ctx) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_INTERNAL, "failed to create query transaction context"
    );
  }

  if (!_queryString.empty()) {
    Parser parser(this);
    parser.parse();

    // put in bind parameters
    parser.ast()->injectBindParameters(_bindParameters, ctx->resolver());
  }

  TRI_ASSERT(_trx == nullptr);

  // TODO: Remove once we have cluster wide transactions
  std::unordered_set<std::string> inaccessibleCollections;
#ifdef USE_ENTERPRISE
  if (_queryOptions.transactionOptions.skipInaccessibleCollections) {
    inaccessibleCollections = _queryOptions.inaccessibleCollections;
  }
#endif

  std::unique_ptr<AqlTransaction> trx(AqlTransaction::create(
    std::move(ctx),
    _collections.collections(),
    _queryOptions.transactionOptions,
    _part == PART_MAIN,
    inaccessibleCollections
  ));
  TRI_DEFER(trx.release());
  // create the transaction object, but do not start it yet
  _trx = trx.get();

  if(!trx->transactionContextPtr()->getParentTransaction()){
    trx->addHint(transaction::Hints::Hint::FROM_TOPLEVEL_AQL);
  }

  // As soon as we start to instantiate the plan we have to clean it
  // up before killing the unique_ptr
  if (!_queryString.empty()) {
    // we have an AST
    // optimize the ast
    enterState(QueryExecutionState::ValueType::AST_OPTIMIZATION);

    _ast->validateAndOptimize();

    enterState(QueryExecutionState::ValueType::LOADING_COLLECTIONS);

    Result res = _trx->begin();

    if (!res.ok()) {
      THROW_ARANGO_EXCEPTION(res);
    }

    enterState(QueryExecutionState::ValueType::PLAN_INSTANTIATION);
    plan = ExecutionPlan::instantiateFromAst(_ast.get());

    if (plan == nullptr) {
      // oops
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "failed to create query execution engine");
    }

    // Run the query optimizer:
    enterState(QueryExecutionState::ValueType::PLAN_OPTIMIZATION);
    arangodb::aql::Optimizer opt(_queryOptions.maxNumberOfPlans);
    // get enabled/disabled rules
    opt.createPlans(std::move(plan), _queryOptions, false);
    // Now plan and all derived plans belong to the optimizer
    plan = opt.stealBest();  // Now we own the best one again
  } else {  // no queryString, we are instantiating from _queryBuilder
    VPackSlice const querySlice = _queryBuilder->slice();
    ExecutionPlan::getCollectionsFromVelocyPack(_ast.get(), querySlice);

    _ast->variables()->fromVelocyPack(querySlice);
    // creating the plan may have produced some collections
    // we need to add them to the transaction now (otherwise the query will
    // fail)

    enterState(QueryExecutionState::ValueType::LOADING_COLLECTIONS);

    Result res = trx->addCollections(*_collections.collections());
    if (res.ok()) {
      res = _trx->begin();
    }

    if (!res.ok()) {
      THROW_ARANGO_EXCEPTION(res);
    }

    enterState(QueryExecutionState::ValueType::PLAN_INSTANTIATION);

    // we have an execution plan in VelocyPack format
    plan.reset(ExecutionPlan::instantiateFromVelocyPack(_ast.get(), _queryBuilder->slice()));
    if (plan == nullptr) {
      // oops
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "could not create plan from vpack");
    }

    plan->findVarUsage();
  }

  TRI_ASSERT(plan != nullptr);

  // return the V8 context if we are in one
  exitContext();

  return plan.release();
}

/// @brief execute an AQL query
ExecutionState Query::execute(QueryRegistry* registry, QueryResult& queryResult) {
  LOG_TOPIC(DEBUG, Logger::QUERIES) << TRI_microtime() - _startTime << " "
                                    << "Query::execute"
                                    << " this: " << (uintptr_t) this;
  TRI_ASSERT(registry != nullptr);

  try {
    bool useQueryCache = canUseQueryCache();

    switch (_executionPhase) {
      case ExecutionPhase::INITIALIZE: {
        if (useQueryCache) {
          // check the query cache for an existing result
          auto cacheEntry = arangodb::aql::QueryCache::instance()->lookup(
            &_vocbase, hash(), _queryString, bindParameters()
          );

          if (cacheEntry != nullptr) {
            bool hasPermissions = true;

            ExecContext const* exe = ExecContext::CURRENT;
            // got a result from the query cache
            if (exe != nullptr) {
              for (std::string const& dataSourceName : cacheEntry->_dataSources) {
                if (!exe->canUseCollection(dataSourceName, auth::Level::RO)) {
                  // cannot use query cache result because of permissions
                  hasPermissions = false;
                  break;
                }
              }
            }

            if (hasPermissions) {
              // we don't have yet a transaction when we're here, so let's create
              // a mimimal context to build the result
              queryResult.context = transaction::StandaloneContext::Create(_vocbase);
              TRI_ASSERT(cacheEntry->_queryResult != nullptr);
              queryResult.result = cacheEntry->_queryResult;
              queryResult.extra = cacheEntry->_stats;
              queryResult.cached = true;

              return ExecutionState::DONE;
            }
            // if no permissions, fall through to regular querying
          }
        }

        // will throw if it fails
        prepare(registry);

        log();

        _resultBuilderOptions = std::make_shared<VPackOptions>(VPackOptions::Defaults);
        _resultBuilderOptions->buildUnindexedArrays = true;
        _resultBuilderOptions->buildUnindexedObjects = true;

        // NOTE: If the options have a shorter lifetime than the builder, it
        // gets invalid (at least set() and close() are broken).
        _resultBuilder = std::make_shared<VPackBuilder>(_resultBuilderOptions.get());

        // reserve some space in Builder to avoid frequent reallocs
        _resultBuilder->reserve(16 * 1024);

        _resultBuilder->openArray();
        _executionPhase = ExecutionPhase::EXECUTE;
      }
      // intentionally falls through
      case ExecutionPhase::EXECUTE: {
        TRI_ASSERT(_resultBuilder != nullptr);
        TRI_ASSERT(_resultBuilder->isOpenArray());
        TRI_ASSERT(_trx != nullptr);

        if (useQueryCache && (_isModificationQuery || !_warnings.empty() ||
                              !_ast->root()->isCacheable())) {
          useQueryCache = false;
        }

        TRI_ASSERT(_engine != nullptr);

        // this is the RegisterId our results can be found in
        RegisterId const resultRegister = _engine->resultRegister();

        // We loop as long as we are having ExecState::DONE returned
        // In case of WAITING we return, this function is repeatable!
        // In case of HASMORE we loop
        while (true) {
          auto res = _engine->getSome(ExecutionBlock::DefaultBatchSize());
          if (res.first == ExecutionState::WAITING) {
            return res.first;
          }

          // res.second == nullptr => state == DONE
          TRI_ASSERT(res.second != nullptr || res.first == ExecutionState::DONE);

          if (res.second == nullptr) {
            TRI_ASSERT(res.first == ExecutionState::DONE);
            break;
          }

          // cache low-level pointer to avoid repeated shared-ptr-derefs
          TRI_ASSERT(_resultBuilder != nullptr);
          auto& resultBuilder = *_resultBuilder;

          size_t const n = res.second->size();

          for (size_t i = 0; i < n; ++i) {
            AqlValue const& val = res.second->getValueReference(i, resultRegister);

            if (!val.isEmpty()) {
              val.toVelocyPack(_trx, resultBuilder, useQueryCache);
            }
          }

          _engine->_itemBlockManager.returnBlock(std::move(res.second));

          if (res.first == ExecutionState::DONE) {
            break;
          }
        }

        // must close result array here because it must be passed as a closed
        // array to the query cache
        _resultBuilder->close();

        if (useQueryCache && _warnings.empty()) {
          // create a query cache entry for later storage
          _cacheEntry = std::make_unique<QueryCacheResultEntry>(
              hash(),
              _queryString,
              _resultBuilder,
              bindParameters(),
              _trx->state()->collectionNames(_views)
          );
        }

        queryResult.result = std::move(_resultBuilder);
        queryResult.context = _trx->transactionContext();
        _executionPhase = ExecutionPhase::FINALIZE;
      }
      // intentionally falls through
      case ExecutionPhase::FINALIZE: {
        // will set warnings, stats, profile and cleanup plan and engine
        return finalize(queryResult);
      }
    }
    // We should not be able to get here
    TRI_ASSERT(false);
    return ExecutionState::DONE;
  } catch (arangodb::basics::Exception const& ex) {
    setExecutionTime();
    cleanupPlanAndEngineSync(ex.code());
    queryResult.set(ex.code(),
        "AQL: " + ex.message() +
        QueryExecutionState::toStringWithPrefix(_state));
    return ExecutionState::DONE;
  } catch (std::bad_alloc const&) {
    setExecutionTime();
    cleanupPlanAndEngineSync(TRI_ERROR_OUT_OF_MEMORY);
    queryResult.set(TRI_ERROR_OUT_OF_MEMORY,
        TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY) +
        QueryExecutionState::toStringWithPrefix(_state));
    return ExecutionState::DONE;
  } catch (std::exception const& ex) {
    setExecutionTime();
    cleanupPlanAndEngineSync(TRI_ERROR_INTERNAL);
    queryResult.set(TRI_ERROR_INTERNAL,
        ex.what() + QueryExecutionState::toStringWithPrefix(_state));
    return ExecutionState::DONE;
  } catch (...) {
    setExecutionTime();
    cleanupPlanAndEngineSync(TRI_ERROR_INTERNAL);
    queryResult.set(TRI_ERROR_INTERNAL,
        TRI_errno_string(TRI_ERROR_INTERNAL) +
        QueryExecutionState::toStringWithPrefix(_state));
    return ExecutionState::DONE;
  }
}

/**
 * @brief Execute the query in a synchronous way, so if
 *        the query needs to wait (e.g. IO) this thread
 *        will be blocked.
 *
 * @param registry The query registry.
 *
 * @return The result of this query. The result is always complete
 */
QueryResult Query::executeSync(QueryRegistry* registry) {
  std::shared_ptr<SharedQueryState> ss = sharedState();
  ss->setContinueCallback();

  QueryResult queryResult;
  while(true) {
    auto state = execute(registry, queryResult);
    if (state != aql::ExecutionState::WAITING) {
      TRI_ASSERT(state == aql::ExecutionState::DONE);
      return queryResult;
    }
    ss->waitForAsyncResponse();
  }
}

// execute an AQL query: may only be called with an active V8 handle scope
ExecutionState Query::executeV8(v8::Isolate* isolate, QueryRegistry* registry, QueryResultV8& queryResult) {
  LOG_TOPIC(DEBUG, Logger::QUERIES) << TRI_microtime() - _startTime << " "
                                    << "Query::executeV8"
                                    << " this: " << (uintptr_t) this;
  TRI_ASSERT(registry != nullptr);

  std::shared_ptr<SharedQueryState> ss = sharedState();
  ss->setContinueCallback();

  try {
    bool useQueryCache = canUseQueryCache();

    if (useQueryCache) {
      // check the query cache for an existing result
      auto cacheEntry = arangodb::aql::QueryCache::instance()->lookup(
        &_vocbase, hash(), _queryString, bindParameters()
      );

      if (cacheEntry != nullptr) {
        bool hasPermissions = true;
        auto ctx = transaction::StandaloneContext::Create(_vocbase);
        ExecContext const* exe = ExecContext::CURRENT;

        // got a result from the query cache
        if (exe != nullptr) {
          for (std::string const& dataSourceName : cacheEntry->_dataSources) {
            if (!exe->canUseCollection(dataSourceName, auth::Level::RO)) {
              // cannot use query cache result because of permissions
              hasPermissions = false;
              break;
            }
          }
        }

        if (hasPermissions) {
          // we don't have yet a transaction when we're here, so let's create
          // a mimimal context to build the result
          queryResult.context = ctx;
          v8::Handle<v8::Value> values =
              TRI_VPackToV8(isolate, cacheEntry->_queryResult->slice(),
                            queryResult.context->getVPackOptions());
          TRI_ASSERT(values->IsArray());
          queryResult.result = v8::Handle<v8::Array>::Cast(values);
          queryResult.extra = cacheEntry->_stats;
          queryResult.cached = true;
          return ExecutionState::DONE;
        }
        // if no permissions, fall through to regular querying
      }
    }

    // will throw if it fails
    prepare(registry);

    log();

    if (useQueryCache && (_isModificationQuery || !_warnings.empty() ||
                          !_ast->root()->isCacheable())) {
      useQueryCache = false;
    }

    v8::Handle<v8::Array> resArray = v8::Array::New(isolate);

    TRI_ASSERT(_engine != nullptr);

    // this is the RegisterId our results can be found in
    auto const resultRegister = _engine->resultRegister();
    std::unique_ptr<AqlItemBlock> value;

    // following options and builder only required for query cache
    VPackOptions options = VPackOptions::Defaults;
    options.buildUnindexedArrays = true;
    options.buildUnindexedObjects = true;
    auto builder = std::make_shared<VPackBuilder>(&options);

    try {
      std::shared_ptr<SharedQueryState> ss = sharedState();

      // iterate over result, return it and optionally store it in query cache
      builder->openArray();

      // iterate over result and return it
      uint32_t j = 0;
      ExecutionState state = ExecutionState::HASMORE;
      while (state != ExecutionState::DONE) {
        auto res = _engine->getSome(ExecutionBlock::DefaultBatchSize());
        state = res.first;
        while (state == ExecutionState::WAITING) {
          ss->waitForAsyncResponse();
          res = _engine->getSome(ExecutionBlock::DefaultBatchSize());
          state = res.first;
        }
        value.swap(res.second);

        // value == nullptr => state == DONE
        TRI_ASSERT(value != nullptr || state == ExecutionState::DONE);

        if (value == nullptr) {
          continue;
        }

        if (!_queryOptions.silent) {
          size_t const n = value->size();

          for (size_t i = 0; i < n; ++i) {
            AqlValue const& val = value->getValueReference(
              i, resultRegister
            );

            if (!val.isEmpty()) {
              resArray->Set(j++, val.toV8(isolate, _trx));

              if (useQueryCache) {
                val.toVelocyPack(_trx, *builder, true);
              }
            }

            if (V8PlatformFeature::isOutOfMemory(isolate)) {
              THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
            }
          }
        }

        _engine->_itemBlockManager.returnBlock(std::move(value));
      }

      builder->close();
    } catch (...) {
      LOG_TOPIC(DEBUG, Logger::QUERIES) << TRI_microtime() - _startTime << " "
                                        << "got an exception executing "
                                        << " this: " << (uintptr_t) this;
      throw;
    }

    queryResult.result = resArray;
    queryResult.context = _trx->transactionContext();

    if (useQueryCache && _warnings.empty()) {
      // create a cache entry for later usage
      _cacheEntry = std::make_unique<QueryCacheResultEntry>(
          hash(),
          _queryString,
          builder,
          bindParameters(),
          _trx->state()->collectionNames(_views)
      );
    }
    // will set warnings, stats, profile and cleanup plan and engine
    ExecutionState state = finalize(queryResult);
    while (state == ExecutionState::WAITING) {
      ss->waitForAsyncResponse();
      state = finalize(queryResult);
    }
  } catch (arangodb::basics::Exception const& ex) {
    setExecutionTime();
    cleanupPlanAndEngineSync(ex.code());
    queryResult.set(ex.code(), "AQL: " + ex.message() + QueryExecutionState::toStringWithPrefix(_state));
  } catch (std::bad_alloc const&) {
    setExecutionTime();
    cleanupPlanAndEngineSync(TRI_ERROR_OUT_OF_MEMORY);
    queryResult.set(
        TRI_ERROR_OUT_OF_MEMORY,
        TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY) + QueryExecutionState::toStringWithPrefix(_state));
  } catch (std::exception const& ex) {
    setExecutionTime();
    cleanupPlanAndEngineSync(TRI_ERROR_INTERNAL);
    queryResult.set(TRI_ERROR_INTERNAL, ex.what() + QueryExecutionState::toStringWithPrefix(_state));
  } catch (...) {
    setExecutionTime();
    cleanupPlanAndEngineSync(TRI_ERROR_INTERNAL);
    queryResult.set(TRI_ERROR_INTERNAL,
                         TRI_errno_string(TRI_ERROR_INTERNAL) + QueryExecutionState::toStringWithPrefix(_state));
  }

  return ExecutionState::DONE;
}

ExecutionState Query::finalize(QueryResult& result) {
  if (result.extra == nullptr || !result.extra->isOpenObject()) {
    // The above condition is not true if we have already waited.
    LOG_TOPIC(DEBUG, Logger::QUERIES) << TRI_microtime() - _startTime << " "
    << "Query::finalize: before _trx->commit"
    << " this: " << (uintptr_t) this;

    Result commitResult = _trx->commit();
    if (commitResult.fail()) {
      THROW_ARANGO_EXCEPTION(commitResult);
    }

    LOG_TOPIC(DEBUG, Logger::QUERIES)
    << TRI_microtime() - _startTime << " "
    << "Query::finalize: before cleanupPlanAndEngine"
    << " this: " << (uintptr_t) this;

    _engine->_stats.setExecutionTime(runTime());
    enterState(QueryExecutionState::ValueType::FINALIZATION);

    result.extra = std::make_shared<VPackBuilder>();
    result.extra->openObject(true);
    if (_queryOptions.profile >= PROFILE_LEVEL_BLOCKS) {
      result.extra->add(VPackValue("plan"));
      _plan->toVelocyPack(*result.extra, _ast.get(), false);
      // needed to happen before plan cleanup
    }
  }

  auto state = cleanupPlanAndEngine(TRI_ERROR_NO_ERROR, result.extra.get());
  if (state == ExecutionState::WAITING) {
    return state;
  }

  addWarningsToVelocyPack(*result.extra);
  double now = TRI_microtime();
  if (_profile != nullptr && _queryOptions.profile >= PROFILE_LEVEL_BASIC) {
    _profile->setStateEnd(QueryExecutionState::ValueType::FINALIZATION, now);
    _profile->toVelocyPack(*(result.extra));
  }
  result.extra->close();

  if (_cacheEntry != nullptr) {
    _cacheEntry->_stats = result.extra;
    QueryCache::instance()->store(&_vocbase, std::move(_cacheEntry));
  }

  // patch executionTime stats value in place
  // we do this because "executionTime" should include the whole span of the execution and we have to set it at the very end
  double const rt = runTime(now);
  basics::VelocyPackHelper::patchDouble(result.extra->slice().get("stats").get("executionTime"), rt);

  LOG_TOPIC(DEBUG, Logger::QUERIES) << rt << " "
  << "Query::finalize:returning"
  << " this: " << (uintptr_t) this;
  return ExecutionState::DONE;
}

/// @brief parse an AQL query
QueryResult Query::parse() {
  try {
    init();
    Parser parser(this);
    return parser.parseWithDetails();
  } catch (arangodb::basics::Exception const& ex) {
    return QueryResult(ex.code(), ex.message());
  } catch (std::bad_alloc const&) {
    cleanupPlanAndEngineSync(TRI_ERROR_OUT_OF_MEMORY);
    return QueryResult(TRI_ERROR_OUT_OF_MEMORY,
                       TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY));
  } catch (std::exception const& ex) {
    cleanupPlanAndEngineSync(TRI_ERROR_INTERNAL);
    return QueryResult(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    return QueryResult(TRI_ERROR_INTERNAL,
                       "an unknown error occurred while parsing the query");
  }
}

/// @brief explain an AQL query
QueryResult Query::explain() {
  try {
    init();
    enterState(QueryExecutionState::ValueType::PARSING);

    auto ctx = createTransactionContext();
    Parser parser(this);
    parser.parse();

    // put in bind parameters
    parser.ast()->injectBindParameters(_bindParameters, ctx->resolver());

    // optimize and validate the ast
    enterState(QueryExecutionState::ValueType::AST_OPTIMIZATION);

    // create the transaction object, but do not start it yet
    _trx = AqlTransaction::create(
      std::move(ctx),
      _collections.collections(),
      _queryOptions.transactionOptions,
      true
    );

    // we have an AST
    Result res = _trx->begin();

    if (!res.ok()) {
      THROW_ARANGO_EXCEPTION(res);
    }

    enterState(QueryExecutionState::ValueType::LOADING_COLLECTIONS);
    parser.ast()->validateAndOptimize();


    enterState(QueryExecutionState::ValueType::PLAN_INSTANTIATION);
    std::unique_ptr<ExecutionPlan> plan = ExecutionPlan::instantiateFromAst(parser.ast());

    if (plan == nullptr) {
      // oops
      return QueryResult(TRI_ERROR_INTERNAL, "unable to create plan from AST");
    }

    // Run the query optimizer:
    enterState(QueryExecutionState::ValueType::PLAN_OPTIMIZATION);
    arangodb::aql::Optimizer opt(_queryOptions.maxNumberOfPlans);
    // get enabled/disabled rules
    opt.createPlans(std::move(plan), _queryOptions, true);

    enterState(QueryExecutionState::ValueType::FINALIZATION);

    QueryResult result(TRI_ERROR_NO_ERROR);

    if (_queryOptions.allPlans) {
      result.result = std::make_shared<VPackBuilder>();
      {
        VPackArrayBuilder guard(result.result.get());

        auto const& plans = opt.getPlans();
        for (auto& it : plans) {
          auto& plan = it.first;
          TRI_ASSERT(plan != nullptr);

          plan->findVarUsage();
          plan->planRegisters();
          plan->toVelocyPack(*result.result.get(), parser.ast(), _queryOptions.verbosePlans);
        }
      }
      // cacheability not available here
      result.cached = false;
    } else {
      // Now plan and all derived plans belong to the optimizer
      std::unique_ptr<ExecutionPlan> bestPlan = opt.stealBest();  // Now we own the best one again
      TRI_ASSERT(bestPlan != nullptr);

      bestPlan->findVarUsage();
      bestPlan->planRegisters();

      result.result = bestPlan->toVelocyPack(parser.ast(), _queryOptions.verbosePlans);

      // cacheability
      result.cached = (!_queryString.empty() &&
                       !_isModificationQuery && _warnings.empty() &&
                       _ast->root()->isCacheable());
    }

    // technically no need to commit, as we are only explaining here
    auto commitResult = _trx->commit();
    if (commitResult.fail()) {
      THROW_ARANGO_EXCEPTION(commitResult);
    }

    result.extra = std::make_shared<VPackBuilder>();
    {
      VPackObjectBuilder guard(result.extra.get(), true);
      addWarningsToVelocyPack(*result.extra);
      result.extra->add(VPackValue("stats"));
      opt._stats.toVelocyPack(*result.extra);
    }

    return result;
  } catch (arangodb::basics::Exception const& ex) {
    return QueryResult(ex.code(), ex.message() + QueryExecutionState::toStringWithPrefix(_state));
  } catch (std::bad_alloc const&) {
    return QueryResult(
        TRI_ERROR_OUT_OF_MEMORY,
        TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY) + QueryExecutionState::toStringWithPrefix(_state));
  } catch (std::exception const& ex) {
    return QueryResult(TRI_ERROR_INTERNAL, ex.what() + QueryExecutionState::toStringWithPrefix(_state));
  } catch (...) {
    return QueryResult(TRI_ERROR_INTERNAL,
                       TRI_errno_string(TRI_ERROR_INTERNAL) + QueryExecutionState::toStringWithPrefix(_state));
  }
}

void Query::setEngine(ExecutionEngine* engine) {
  TRI_ASSERT(engine != nullptr);
  _engine.reset(engine);
}

/// @brief prepare a V8 context for execution for this expression
/// this needs to be called once before executing any V8 function in this
/// expression
void Query::prepareV8Context() {
  if (_preparedV8Context) {
    // already done
    return;
  }

  TRI_ASSERT(_trx != nullptr);

  ISOLATE;
  TRI_ASSERT(isolate != nullptr);

  std::string body("if (_AQL === undefined) { _AQL = require(\"@arangodb/aql\"); }");

  {
    v8::HandleScope scope(isolate);
    v8::Handle<v8::Script> compiled = v8::Script::Compile(
        TRI_V8_STD_STRING(isolate, body), TRI_V8_ASCII_STRING(isolate, "--script--"));

    if (!compiled.IsEmpty()) {
      compiled->Run();
      _preparedV8Context = true;
    }
  }
}

/// @brief enter a V8 context
void Query::enterContext() {
  if (!_contextOwnedByExterior) {
    if (_context == nullptr) {
      if (V8DealerFeature::DEALER == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "V8 engine is disabled");
      }
      TRI_ASSERT(V8DealerFeature::DEALER != nullptr);
      _context = V8DealerFeature::DEALER->enterContext(&_vocbase, false);

      if (_context == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_RESOURCE_LIMIT,
                                       "unable to enter V8 context for query execution");
      }

      // register transaction and resolver in context
      TRI_ASSERT(_trx != nullptr);

      ISOLATE;
      TRI_GET_GLOBALS();
      _preparedV8Context = false;
      auto ctx = static_cast<arangodb::transaction::V8Context*>(
          v8g->_transactionContext);
      if (ctx != nullptr) {
        ctx->registerTransaction(_trx->state());
      }
    }
    _preparedV8Context = false;

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
      auto ctx = static_cast<arangodb::transaction::V8Context*>(
          v8g->_transactionContext);
      if (ctx != nullptr) {
        ctx->unregisterTransaction();
      }

      TRI_ASSERT(V8DealerFeature::DEALER != nullptr);
      V8DealerFeature::DEALER->exitContext(_context);
      _context = nullptr;
    }
    _preparedV8Context = false;
  }
}

/// @brief returns statistics for current query.
void Query::getStats(VPackBuilder& builder) {
  if (_engine != nullptr) {
    _engine->_stats.setExecutionTime(TRI_microtime() - _startTime);
    _engine->_stats.toVelocyPack(builder, _queryOptions.fullCount);
  } else {
    ExecutionStats::toVelocyPackStatic(builder);
  }
}

/// @brief convert the list of warnings to VelocyPack.
///        Will add a new entry { ..., warnings: <warnings>, } if there are
///        warnings. If there are none it will not modify the builder
void Query::addWarningsToVelocyPack(VPackBuilder& builder) const {
  TRI_ASSERT(builder.isOpenObject());
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

/// @brief initializes the query
void Query::init() {
  if (_id != 0) {
    // already called
    return;
  }
  TRI_ASSERT(_id == 0);
  _id = nextId();
  TRI_ASSERT(_id != 0);

  TRI_ASSERT(_profile == nullptr);
  // adds query to QueryList which is needed for /_api/query/current
  _profile.reset(new QueryProfile(this));
  enterState(QueryExecutionState::ValueType::INITIALIZATION);

  TRI_ASSERT(_ast == nullptr);
  _ast.reset(new Ast(this));
}

/// @brief calculate a hash for the query, once
uint64_t Query::hash() const {
  if (!_queryHashCalculated) {
    _queryHash = calculateHash();
    _queryHashCalculated = true;
  }
  return _queryHash;
}

/// @brief log a query
void Query::log() {
  if (!_queryString.empty()) {
    LOG_TOPIC(TRACE, Logger::QUERIES)
        << "executing query " << _id << ": '" << _queryString.extract(1024) << "'";
  }
}

/// @brief calculate a hash value for the query and bind parameters
uint64_t Query::calculateHash() const {
  if (_queryString.empty()) {
    return DontCache;
  }

  // hash the query string first
  uint64_t hash = _queryString.hash();

  // handle "fullCount" option. if this option is set, the query result will
  // be different to when it is not set!
  if (_queryOptions.fullCount) {
    hash = fasthash64(TRI_CHAR_LENGTH_PAIR("fullcount:true"), hash);
  } else {
    hash = fasthash64(TRI_CHAR_LENGTH_PAIR("fullcount:false"), hash);
  }

  // handle "count" option
  if (_queryOptions.count) {
    hash = fasthash64(TRI_CHAR_LENGTH_PAIR("count:true"), hash);
  } else {
    hash = fasthash64(TRI_CHAR_LENGTH_PAIR("count:false"), hash);
  }

  // also hash "optimizer" options
  VPackSlice options = arangodb::velocypack::Slice::emptyObjectSlice();

  if (_options != nullptr && _options->slice().isObject()) {
    options = _options->slice().get("optimizer");
  }
  hash ^= options.hash();

  // blend query hash with bind parameters
  return hash ^ _bindParameters.hash();
}

/// @brief whether or not the query cache can be used for the query
bool Query::canUseQueryCache() const {
  if (_queryString.size() < 8) {
    return false;
  }
  if (_isModificationQuery) {
    return false;
  }

  auto queryCacheMode = QueryCache::instance()->mode();

  if (_queryOptions.cache &&
      (queryCacheMode == CACHE_ALWAYS_ON ||
       queryCacheMode == CACHE_ON_DEMAND)) {
    // cache mode is set to always on or on-demand...
    // query will only be cached if `cache` attribute is not set to false

    // cannot use query cache on a coordinator at the moment
    return !arangodb::ServerState::instance()->isRunningInCluster();
  }

  return false;
}

/// @brief neatly format exception messages for the users
std::string Query::buildErrorMessage(int errorCode) const {
  std::string err(TRI_errno_string(errorCode));

  if (!_queryString.empty() && _queryOptions.verboseErrors) {
    err += "\nwhile executing:\n";
    _queryString.append(err);
    err += "\n";
  }

  return err;
}

/// @brief enter a new state
void Query::enterState(QueryExecutionState::ValueType state) {
  LOG_TOPIC(DEBUG, Logger::QUERIES) << TRI_microtime() - _startTime << " "
                                    << "Query::enterState: " << state
                                    << " this: " << (uintptr_t) this;
  if (_profile != nullptr) {
    // record timing for previous state
    _profile->setStateDone(_state);
  }

  // and adjust the state
  _state = state;
}

void Query::cleanupPlanAndEngineSync(int errorCode, VPackBuilder* statsBuilder) noexcept {
  try {
    std::shared_ptr<SharedQueryState> ss = sharedState();
    ss->setContinueCallback();

    ExecutionState state = cleanupPlanAndEngine(errorCode, statsBuilder);
    while (state == ExecutionState::WAITING) {
      ss->waitForAsyncResponse();
      state = cleanupPlanAndEngine(errorCode, statsBuilder);
    }
  } catch (...) {
    // this is called from the destructor... we must not leak exceptions from here
  }
}

/// @brief cleanup plan and engine for current query
ExecutionState Query::cleanupPlanAndEngine(int errorCode, VPackBuilder* statsBuilder) {
  if (_engine != nullptr) {
    try {
      ExecutionState state;
      std::tie(state, std::ignore) = _engine->shutdown(errorCode);
      if (state == ExecutionState::WAITING) {
        return state;
      }
      if (statsBuilder != nullptr) {
        TRI_ASSERT(statsBuilder->isOpenObject());
        statsBuilder->add(VPackValue("stats"));
        _engine->_stats.toVelocyPack(*statsBuilder, _queryOptions.fullCount);
      }
    } catch (...) {
      // shutdown may fail but we must not throw here
      // (we're also called from the destructor)
    }

    _sharedState->invalidate();
    _engine.reset();
  }

  // If the transaction was not committed, it is automatically aborted
  delete _trx;
  _trx = nullptr;

  _plan.reset();
  return ExecutionState::DONE;
}

/// @brief create a transaction::Context
std::shared_ptr<transaction::Context> Query::createTransactionContext() {
  if (!_transactionContext) {
    if (_contextOwnedByExterior) {
      // we must use v8
      _transactionContext = transaction::V8Context::Create(_vocbase, true);
    } else {
      _transactionContext = transaction::StandaloneContext::Create(_vocbase);
    }
  }

  TRI_ASSERT(_transactionContext != nullptr);

  return _transactionContext;
}

/// @brief pass-thru a resolver object from the transaction context
CollectionNameResolver const& Query::resolver() {
  return createTransactionContext()->resolver();
}

/// @brief look up a graph either from our cache list or from the _graphs
///        collection
graph::Graph const* Query::lookupGraphByName(std::string const& name) {
  auto it = _graphs.find(name);

  if (it != _graphs.end()) {
    return it->second.get();
  }
  graph::GraphManager graphManager{_vocbase, _contextOwnedByExterior};

  auto g = graphManager.lookupGraphByName(name);

  if (g.fail()) {
    return nullptr;
  }

  auto graph = g.get().get();
  _graphs.emplace(name, std::move(g.get()));

  return graph;
}

/// @brief returns the next query id
TRI_voc_tick_t Query::nextId() {
  return ::nextQueryId.fetch_add(1, std::memory_order_seq_cst);
}
