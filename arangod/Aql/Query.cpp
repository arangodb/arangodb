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
#include "Basics/Exceptions.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/fasthash.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Logger/Logger.h"
#include "RestServer/AqlFeature.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Transaction/V8Context.h"
#include "Utils/ExecContext.h"
#include "V8/v8-conv.h"
#include "V8/v8-vpack.h"
#include "V8Server/V8DealerFeature.h"
#include "VocBase/Graphs.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#ifndef USE_PLAN_CACHE
#undef USE_PLAN_CACHE
#endif

using namespace arangodb;
using namespace arangodb::aql;

namespace {
static std::atomic<TRI_voc_tick_t> NextQueryId(1);
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
      _preparedV8Context(false) {
  AqlFeature* aql = AqlFeature::lease();

  if (aql == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
  }

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
      _preparedV8Context(false) {
  AqlFeature* aql = AqlFeature::lease();

  if (aql == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
  }

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

  _resourceMonitor.setMemoryLimit(_queryOptions.memoryLimit);
}

/// @brief destroys a query
Query::~Query() {
  if (_queryOptions.profile >= PROFILE_LEVEL_TRACE_1) {
    LOG_TOPIC(INFO, Logger::QUERIES)
      << TRI_microtime() - _startTime << " "
      << "Query::~Query queryString: "
      << " this: " << (uintptr_t) this;
  }
  cleanupPlanAndEngine(TRI_ERROR_INTERNAL);  // abort the transaction

  exitContext();

  _ast.reset();

  for (auto& it : _graphs) {
    delete it.second;
  }
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

void Query::prepare(QueryRegistry* registry, uint64_t queryHash) {
  TRI_ASSERT(registry != nullptr);

  init();
  enterState(QueryExecutionState::ValueType::PARSING);

  std::unique_ptr<ExecutionPlan> plan;

#if USE_PLAN_CACHE
  if (!_queryString.empty() &&
      queryHash != DontCache &&
      _part == PART_MAIN) {
    // LOG_TOPIC(INFO, Logger::FIXME) << "trying to find query in execution plan cache: '" << _queryString << "', hash: " << queryHash;

    // store & lookup velocypack plans!!
    std::shared_ptr<PlanCacheEntry> planCacheEntry = PlanCache::instance()->lookup(_vocbase, queryHash, queryString);
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
        queryHash != DontCache &&
        _part == PART_MAIN &&
        _warnings.empty() &&
        _ast->root()->isCacheable()) {
      // LOG_TOPIC(INFO, Logger::FIXME) << "storing query in execution plan cache '" << _queryString << "', hash: " << queryHash;
      PlanCache::instance()->store(_vocbase, queryHash, _queryString, plan.get());
    }
#endif
  }

  TRI_ASSERT(plan != nullptr);
  if (!plan->varUsageComputed()) {
    plan->findVarUsage();
  }

  enterState(QueryExecutionState::ValueType::EXECUTION);

  TRI_ASSERT(_engine == nullptr);
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
  std::unique_ptr<ExecutionPlan> plan;

  if (!_queryString.empty()) {
    Parser parser(this);

    parser.parse(false);
    // put in bind parameters
    parser.ast()->injectBindParameters(_bindParameters);
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
                     createTransactionContext(), _collections.collections(),
                     _queryOptions.transactionOptions,
                     _part == PART_MAIN, inaccessibleCollections));
  TRI_DEFER(trx.release());
  // create the transaction object, but do not start it yet
  _trx = trx.get();

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
    plan.reset(ExecutionPlan::instantiateFromAst(_ast.get()));

    if (plan == nullptr) {
      // oops
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "failed to create query execution engine");
    }

    // Run the query optimizer:
    enterState(QueryExecutionState::ValueType::PLAN_OPTIMIZATION);
    arangodb::aql::Optimizer opt(_queryOptions.maxNumberOfPlans);
    // get enabled/disabled rules
    opt.createPlans(plan.release(), _queryOptions.optimizerRules,
                    _queryOptions.inspectSimplePlans, false);
    // Now plan and all derived plans belong to the optimizer
    plan.reset(opt.stealBest());  // Now we own the best one again
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

    if (!plan->varUsageComputed()) {
      plan->findVarUsage();
    }
  }

  TRI_ASSERT(plan != nullptr);

  // return the V8 context if we are in one
  exitContext();

  return plan.release();
}

/// @brief execute an AQL query
QueryResult Query::execute(QueryRegistry* registry) {
  LOG_TOPIC(DEBUG, Logger::QUERIES) << TRI_microtime() - _startTime << " "
                                    << "Query::execute"
                                    << " this: " << (uintptr_t) this;
  TRI_ASSERT(registry != nullptr);

  try {
    bool useQueryCache = canUseQueryCache();
    uint64_t queryHash = hash();

    if (useQueryCache) {
      // check the query cache for an existing result
      auto cacheEntry = arangodb::aql::QueryCache::instance()->lookup(
        &_vocbase, queryHash, _queryString
      );
      arangodb::aql::QueryCacheResultEntryGuard guard(cacheEntry);

      if (cacheEntry != nullptr) {
        ExecContext const* exe = ExecContext::CURRENT;
        // got a result from the query cache
        if(exe != nullptr) {
          for (std::string const& collectionName : cacheEntry->_collections) {
            if (!exe->canUseCollection(collectionName, auth::Level::RO)) {
              THROW_ARANGO_EXCEPTION(TRI_ERROR_FORBIDDEN);
            }
          }
        }

        QueryResult res;

        // we don't have yet a transaction when we're here, so let's create
        // a mimimal context to build the result
        res.context = transaction::StandaloneContext::Create(_vocbase);
        res.extra = std::make_shared<VPackBuilder>();
        {
          VPackObjectBuilder guard(res.extra.get(), true);
          addWarningsToVelocyPack(*res.extra);
        }
        TRI_ASSERT(cacheEntry->_queryResult != nullptr);
        res.result = cacheEntry->_queryResult;
        res.cached = true;

        return res;
      }
    }

    // will throw if it fails
    prepare(registry, queryHash);

    log();
    TRI_ASSERT(_trx != nullptr);

    if (useQueryCache && (_isModificationQuery || !_warnings.empty() ||
                          !_ast->root()->isCacheable())) {
      useQueryCache = false;
    }

    VPackOptions options = VPackOptions::Defaults;
    options.buildUnindexedArrays = true;
    options.buildUnindexedObjects = true;

    auto resultBuilder = std::make_shared<VPackBuilder>(&options);
    resultBuilder->reserve(
        16 * 1024);  // reserve some space in Builder to avoid frequent reallocs

    TRI_ASSERT(_engine != nullptr);

    // this is the RegisterId our results can be found in
    RegisterId const resultRegister = _engine->resultRegister();
    AqlItemBlock* value = nullptr;

    try {
      resultBuilder->openArray();

      // iterate over result, return it and store it in query cache
      while (nullptr != (value = _engine->getSome(ExecutionBlock::DefaultBatchSize()))) {
        size_t const n = value->size();

        for (size_t i = 0; i < n; ++i) {
          AqlValue const& val = value->getValueReference(i, resultRegister);

          if (!val.isEmpty()) {
            val.toVelocyPack(_trx, *resultBuilder, useQueryCache);
          }
        }
        _engine->_itemBlockManager.returnBlock(value);
      }

      // must close result array here because it must be passed as a closed
      // array to the query cache
      resultBuilder->close();

      if (useQueryCache) {
        if (_warnings.empty()) {
          // finally store the generated result in the query cache
          auto result = QueryCache::instance()->store(
            &_vocbase,
            queryHash,
            _queryString,
            resultBuilder,
            _trx->state()->collectionNames()
          );

          if (result == nullptr) {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
          }
        }
      }
    } catch (...) {
      delete value;
      throw;
    }

    QueryResult result;
    result.result = std::move(resultBuilder);
    result.context = _trx->transactionContext();
    // will set warnings, stats, profile and cleanup plan and engine
    finalize(result);

    return result;
  } catch (arangodb::basics::Exception const& ex) {
    setExecutionTime();
    cleanupPlanAndEngine(ex.code());
    return QueryResult(ex.code(), "AQL: " + ex.message() + QueryExecutionState::toStringWithPrefix(_state));
  } catch (std::bad_alloc const&) {
    setExecutionTime();
    cleanupPlanAndEngine(TRI_ERROR_OUT_OF_MEMORY);
    return QueryResult(
        TRI_ERROR_OUT_OF_MEMORY,
        TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY) + QueryExecutionState::toStringWithPrefix(_state));
  } catch (std::exception const& ex) {
    setExecutionTime();
    cleanupPlanAndEngine(TRI_ERROR_INTERNAL);
    return QueryResult(TRI_ERROR_INTERNAL, ex.what() + QueryExecutionState::toStringWithPrefix(_state));
  } catch (...) {
    setExecutionTime();
    cleanupPlanAndEngine(TRI_ERROR_INTERNAL);
    return QueryResult(TRI_ERROR_INTERNAL,
                       TRI_errno_string(TRI_ERROR_INTERNAL) + QueryExecutionState::toStringWithPrefix(_state));
  }
}

// execute an AQL query: may only be called with an active V8 handle scope
QueryResultV8 Query::executeV8(v8::Isolate* isolate, QueryRegistry* registry) {
  LOG_TOPIC(DEBUG, Logger::QUERIES) << TRI_microtime() - _startTime << " "
                                    << "Query::executeV8"
                                    << " this: " << (uintptr_t) this;
  TRI_ASSERT(registry != nullptr);

  try {
    bool useQueryCache = canUseQueryCache();
    uint64_t queryHash = hash();

    if (useQueryCache) {
      // check the query cache for an existing result
      auto cacheEntry = arangodb::aql::QueryCache::instance()->lookup(
        &_vocbase, queryHash, _queryString
      );
      arangodb::aql::QueryCacheResultEntryGuard guard(cacheEntry);

      if (cacheEntry != nullptr) {
        auto ctx = transaction::StandaloneContext::Create(_vocbase);
        ExecContext const* exe = ExecContext::CURRENT;

        // got a result from the query cache
        if(exe != nullptr) {
          for (std::string const& collectionName : cacheEntry->_collections) {
            if (!exe->canUseCollection(collectionName, auth::Level::RO)) {
              THROW_ARANGO_EXCEPTION(TRI_ERROR_FORBIDDEN);
            }
          }
        }

        QueryResultV8 result;
        // we don't have yet a transaction when we're here, so let's create
        // a mimimal context to build the result
        result.context = ctx;
        v8::Handle<v8::Value> values =
            TRI_VPackToV8(isolate, cacheEntry->_queryResult->slice(),
                          result.context->getVPackOptions());
        TRI_ASSERT(values->IsArray());
        result.result = v8::Handle<v8::Array>::Cast(values);
        result.cached = true;
        return result;
      }
    }

    // will throw if it fails
    prepare(registry, queryHash);

    log();

    if (useQueryCache && (_isModificationQuery || !_warnings.empty() ||
                          !_ast->root()->isCacheable())) {
      useQueryCache = false;
    }

    v8::Handle<v8::Array> resArray = v8::Array::New(isolate);

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
        while (nullptr != (value = _engine->getSome(ExecutionBlock::DefaultBatchSize()))) {
          size_t const n = value->size();

          for (size_t i = 0; i < n; ++i) {
            AqlValue const& val = value->getValueReference(i, resultRegister);

            if (!val.isEmpty()) {
              resArray->Set(j++, val.toV8(isolate, _trx));
              val.toVelocyPack(_trx, *builder, true);
            }
          }
          _engine->_itemBlockManager.returnBlock(value);
        }

        builder->close();

        if (_warnings.empty()) {
          // finally store the generated result in the query cache
          QueryCache::instance()->store(
            &_vocbase,
            queryHash,
            _queryString,
            builder,
            _trx->state()->collectionNames()
          );
        }
      } else {
        // iterate over result and return it
        uint32_t j = 0;
        while (nullptr != (value = _engine->getSome(ExecutionBlock::DefaultBatchSize()))) {
          if (!_queryOptions.silent) {
            size_t const n = value->size();

            for (size_t i = 0; i < n; ++i) {
              AqlValue const& val = value->getValueReference(i, resultRegister);

              if (!val.isEmpty()) {
                resArray->Set(j++, val.toV8(isolate, _trx));
              }

              if (V8PlatformFeature::isOutOfMemory(isolate)) {
                THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
              }
            }
          }
          _engine->_itemBlockManager.returnBlock(value);
        }
      }
    } catch (...) {
      LOG_TOPIC(DEBUG, Logger::QUERIES) << TRI_microtime() - _startTime << " "
                                        << "got an exception executing "
                                        << " this: " << (uintptr_t) this;
      delete value;
      throw;
    }

    QueryResultV8 result;
    result.result = resArray;
    result.context = _trx->transactionContext();
    // will set warnings, stats, profile and cleanup plan and engine
    finalize(result);

    return result;
  } catch (arangodb::basics::Exception const& ex) {
    setExecutionTime();
    cleanupPlanAndEngine(ex.code());
    return QueryResultV8(ex.code(), "AQL: " + ex.message() + QueryExecutionState::toStringWithPrefix(_state));
  } catch (std::bad_alloc const&) {
    setExecutionTime();
    cleanupPlanAndEngine(TRI_ERROR_OUT_OF_MEMORY);
    return QueryResultV8(
        TRI_ERROR_OUT_OF_MEMORY,
        TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY) + QueryExecutionState::toStringWithPrefix(_state));
  } catch (std::exception const& ex) {
    setExecutionTime();
    cleanupPlanAndEngine(TRI_ERROR_INTERNAL);
    return QueryResultV8(TRI_ERROR_INTERNAL, ex.what() + QueryExecutionState::toStringWithPrefix(_state));
  } catch (...) {
    setExecutionTime();
    cleanupPlanAndEngine(TRI_ERROR_INTERNAL);
    return QueryResultV8(TRI_ERROR_INTERNAL,
                         TRI_errno_string(TRI_ERROR_INTERNAL) + QueryExecutionState::toStringWithPrefix(_state));
  }
}

void Query::finalize(QueryResult& result) {

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

  double now;
  result.extra = std::make_shared<VPackBuilder>();
  {
    VPackObjectBuilder extras(result.extra.get(), true);
    if (_queryOptions.profile >= PROFILE_LEVEL_BLOCKS) {
      result.extra->add(VPackValue("plan"));
      _plan->toVelocyPack(*result.extra, _ast.get(), false);
      // needed to happen before plan cleanup
    }
    cleanupPlanAndEngine(TRI_ERROR_NO_ERROR, result.extra.get());
    addWarningsToVelocyPack(*result.extra);
    now = TRI_microtime();
    if (_profile != nullptr && _queryOptions.profile >= PROFILE_LEVEL_BASIC) {
      _profile->setStateEnd(QueryExecutionState::ValueType::FINALIZATION, now);
      _profile->toVelocyPack(*(result.extra));
    }
  }

  // patch executionTime stats value in place
  // we do this because "executionTime" should include the whole span of the execution and we have to set it at the very end
  double const rt = runTime(now);
  basics::VelocyPackHelper::patchDouble(result.extra->slice().get("stats").get("executionTime"), rt);

  LOG_TOPIC(DEBUG, Logger::QUERIES) << rt << " "
  << "Query::finalize:returning"
  << " this: " << (uintptr_t) this;
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
    return QueryResult(TRI_ERROR_INTERNAL,
                       "an unknown error occurred while parsing the query");
  }
}

/// @brief explain an AQL query
QueryResult Query::explain() {
  try {
    init();
    enterState(QueryExecutionState::ValueType::PARSING);

    Parser parser(this);

    parser.parse(true);
    // put in bind parameters
    parser.ast()->injectBindParameters(_bindParameters);

    // optimize and validate the ast
    enterState(QueryExecutionState::ValueType::AST_OPTIMIZATION);

    // create the transaction object, but do not start it yet
    _trx = AqlTransaction::create(createTransactionContext(),
                              _collections.collections(),
                              _queryOptions.transactionOptions, true);

    // we have an AST
    Result res = _trx->begin();

    if (!res.ok()) {
      THROW_ARANGO_EXCEPTION(res);
    }

    enterState(QueryExecutionState::ValueType::LOADING_COLLECTIONS);
    parser.ast()->validateAndOptimize();


    enterState(QueryExecutionState::ValueType::PLAN_INSTANTIATION);
    ExecutionPlan* plan = ExecutionPlan::instantiateFromAst(parser.ast());

    if (plan == nullptr) {
      // oops
      return QueryResult(TRI_ERROR_INTERNAL, "unable to create plan from AST");
    }

    // Run the query optimizer:
    enterState(QueryExecutionState::ValueType::PLAN_OPTIMIZATION);
    arangodb::aql::Optimizer opt(_queryOptions.maxNumberOfPlans);
    // get enabled/disabled rules
    opt.createPlans(plan, _queryOptions.optimizerRules, _queryOptions.inspectSimplePlans, true);

    enterState(QueryExecutionState::ValueType::FINALIZATION);

    QueryResult result(TRI_ERROR_NO_ERROR);

    if (_queryOptions.allPlans) {
      result.result = std::make_shared<VPackBuilder>();
      {
        VPackArrayBuilder guard(result.result.get());

        auto plans = opt.getPlans();
        for (auto& it : plans) {
          TRI_ASSERT(it != nullptr);

          it->findVarUsage();
          it->planRegisters();
          it->toVelocyPack(*result.result.get(), parser.ast(), _queryOptions.verbosePlans);
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

void Query::releaseEngine() {
  _engine.release();
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
  _id = Query::NextId();
  TRI_ASSERT(_id != 0);

  TRI_ASSERT(_profile == nullptr);
  // adds query to QueryList which is needed for /_api/query/current
  _profile.reset(new QueryProfile(this));
  enterState(QueryExecutionState::ValueType::INITIALIZATION);

  TRI_ASSERT(_ast == nullptr);
  _ast.reset(new Ast(this));
}

/// @brief log a query
void Query::log() {
  if (!_queryString.empty()) {
    LOG_TOPIC(TRACE, Logger::QUERIES)
        << "executing query " << _id << ": '" << _queryString.extract(1024) << "'";
  }
}

/// @brief calculate a hash value for the query and bind parameters
uint64_t Query::hash() {
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
  VPackSlice options = basics::VelocyPackHelper::EmptyObjectValue();

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

/// @brief cleanup plan and engine for current query
void Query::cleanupPlanAndEngine(int errorCode, VPackBuilder* statsBuilder) {
  if (_engine != nullptr) {
    try {
      _engine->shutdown(errorCode);
      if (statsBuilder != nullptr) {
        TRI_ASSERT(statsBuilder->isOpenObject());
        statsBuilder->add(VPackValue("stats"));
        _engine->_stats.toVelocyPack(*statsBuilder, _queryOptions.fullCount);
      }
    } catch (...) {
      // shutdown may fail but we must not throw here
      // (we're also called from the destructor)
    }
    _engine.reset();
  }

  if (_trx != nullptr) {
    // If the transaction was not committed, it is automatically aborted
    delete _trx;
    _trx = nullptr;
  }

  _plan.reset();
}

/// @brief create a transaction::Context
std::shared_ptr<transaction::Context> Query::createTransactionContext() {
  if (_contextOwnedByExterior) {
    // we must use v8
    return transaction::V8Context::Create(_vocbase, true);
  }

  return transaction::StandaloneContext::Create(_vocbase);
}

/// @brief look up a graph either from our cache list or from the _graphs
///        collection
Graph const* Query::lookupGraphByName(std::string const& name) {
  auto it = _graphs.find(name);

  if (it != _graphs.end()) {
    return it->second;
  }

  std::unique_ptr<arangodb::aql::Graph> g(
      arangodb::lookupGraphByName(createTransactionContext(), name));

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

