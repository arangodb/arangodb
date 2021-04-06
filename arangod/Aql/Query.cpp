////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include "Aql/AqlCallList.h"
#include "Aql/AqlCallStack.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/AqlTransaction.h"
#include "Aql/Ast.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Optimizer.h"
#include "Aql/Parser.h"
#include "Aql/PlanCache.h"
#include "Aql/QueryCache.h"
#include "Aql/QueryExecutionState.h"
#include "Aql/QueryProfile.h"
#include "Aql/QueryRegistry.h"
#include "Aql/Timing.h"
#include "Basics/Exceptions.h"
#include "Basics/ResourceUsage.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/fasthash.h"
#include "Cluster/ServerState.h"
#include "Graph/Graph.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "Logger/LogMacros.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "Network/Utils.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "StorageEngine/TransactionCollection.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/StandaloneContext.h"
#include "Transaction/V8Context.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/ExecContext.h"
#include "Utils/Events.h"
#include "V8/JavaScriptSecurityContext.h"
#include "V8/v8-vpack.h"
#include "V8Server/V8DealerFeature.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"

#include <velocypack/Iterator.h>

#include <optional>

#ifndef USE_PLAN_CACHE
#undef USE_PLAN_CACHE
#endif

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::basics;

/// @brief internal constructor, Used to construct a full query or a ClusterQuery
Query::Query(std::shared_ptr<transaction::Context> const& ctx,
             QueryString const& queryString, std::shared_ptr<VPackBuilder> const& bindParameters,
             aql::QueryOptions&& options,
             std::shared_ptr<SharedQueryState> sharedState)
    : QueryContext(ctx->vocbase()),
      _itemBlockManager(_resourceMonitor, SerializationFormat::SHADOWROWS),
      _queryString(queryString),
      _transactionContext(ctx),
      _sharedState(std::move(sharedState)),
      _v8Context(nullptr),
      _bindParameters(_resourceMonitor, bindParameters),
      _queryOptions(std::move(options)),
      _trx(nullptr),
      _startTime(currentSteadyClockValue()),
      _resultMemoryUsage(0),
      _queryHash(DontCache),
      _shutdownState(ShutdownState::None),
      _executionPhase(ExecutionPhase::INITIALIZE),
      _resultCode(std::nullopt),
      _contextOwnedByExterior(ctx->isV8Context() && v8::Isolate::GetCurrent() != nullptr),
      _embeddedQuery(ctx->isV8Context() && transaction::V8Context::isEmbedded()),
      _registeredInV8Context(false),
      _queryKilled(false),
      _queryHashCalculated(false) {

  if (!_transactionContext) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "failed to create query transaction context");
  }

  if (_contextOwnedByExterior) {
    // copy transaction options from global state into our local query options
    auto state = transaction::V8Context::getParentState();
    if (state != nullptr) {
      _queryOptions.transactionOptions = state->options();
    }
  }

  ProfileLevel level = _queryOptions.profile;
  if (level >= ProfileLevel::TraceOne) {
    LOG_TOPIC("22a70", INFO, Logger::QUERIES) << elapsedSince(_startTime)
                                              << " Query::Query queryString: " << _queryString
                                              << " this: " << (uintptr_t)this;
  } else {
    LOG_TOPIC("11160", DEBUG, Logger::QUERIES)
        << elapsedSince(_startTime)
        << " Query::Query queryString: " << _queryString << " this: " << (uintptr_t)this;
  }

  if (bindParameters != nullptr && !bindParameters->isEmpty() &&
      !bindParameters->slice().isNone()) {
    if (level >= ProfileLevel::TraceOne) {
      LOG_TOPIC("8c9fc", INFO, Logger::QUERIES)
          << "bindParameters: " << bindParameters->slice().toJson();
    } else {
      LOG_TOPIC("16a3f", DEBUG, Logger::QUERIES)
          << "bindParameters: " << bindParameters->slice().toJson();
    }
  }

  if (level >= ProfileLevel::TraceOne) {
    VPackBuilder b;
    _queryOptions.toVelocyPack(b, /*disableOptimizerRules*/ false);
    LOG_TOPIC("8979d", INFO, Logger::QUERIES) << "options: " << b.toJson();
  }

  // set memory limit for query
  _resourceMonitor.memoryLimit(_queryOptions.memoryLimit);
  _warnings.updateOptions(_queryOptions);
  
  // store name of user that started the query
  _user = ExecContext::current().user();
}

/// @brief public constructor, Used to construct a full query
Query::Query(std::shared_ptr<transaction::Context> const& ctx,
             QueryString const& queryString, std::shared_ptr<VPackBuilder> const& bindParameters,
             QueryOptions&& options)
    : Query(ctx, queryString, bindParameters, std::move(options),
            std::make_shared<SharedQueryState>(ctx->vocbase().server())) {}

Query::Query(std::shared_ptr<transaction::Context> const& ctx,
             QueryString const& queryString, std::shared_ptr<VPackBuilder> const& bindParameters,
             VPackSlice options)
    : Query(ctx, queryString, bindParameters,
            QueryOptions(options),
            std::make_shared<SharedQueryState>(ctx->vocbase().server())) {}

/// @brief destroys a query
Query::~Query() {
  _resourceMonitor.decreaseMemoryUsage(_resultMemoryUsage);
  _resultMemoryUsage = 0;

  if (_queryOptions.profile >= ProfileLevel::TraceOne) {
    LOG_TOPIC("36a75", INFO, Logger::QUERIES) << elapsedSince(_startTime)
                                              << " Query::~Query queryString: "
                                              << " this: " << (uintptr_t)this;
  }

  _queryProfile.reset(); // unregister from QueryList
  
  // log to audit log
  if (!_queryOptions.skipAudit &&
     (ServerState::instance()->isCoordinator() ||
      ServerState::instance()->isSingleServer())) {
    try {
      events::AqlQuery(*this);
    } catch (...) {
      // we must not make any exception escape from here!
    }
  }

  // this will reset _trx, so _trx is invalid after here
  try {
    ExecutionState state = cleanupPlanAndEngine(TRI_ERROR_INTERNAL, /*sync*/true);
    TRI_ASSERT(state != ExecutionState::WAITING);
  } catch (...) {
    // unfortunately we cannot do anything here, as we are in 
    // the destructor
  }
  
  unregisterSnippets();

  exitV8Context();

  _snippets.clear(); // simon: must be before plan
  _plans.clear(); // simon: must be before AST
  _ast.reset();
  
  LOG_TOPIC("f5cee", DEBUG, Logger::QUERIES)
      << elapsedSince(_startTime)
      << " Query::~Query this: " << (uintptr_t)this;
}
  
/// @brief return the user that started the query
std::string const& Query::user() const {
  return _user;
}

double Query::getLockTimeout() const noexcept {
  return _queryOptions.transactionOptions.lockTimeout;
}

void Query::setLockTimeout(double timeout) noexcept {
  _queryOptions.transactionOptions.lockTimeout = timeout;
}

bool Query::killed() const {
  if (_queryOptions.maxRuntime > std::numeric_limits<double>::epsilon() &&
      elapsedSince(_startTime) > _queryOptions.maxRuntime) {
    return true;
  }
  return _queryKilled;
}

/// @brief set the query to killed
void Query::kill() {
  _queryKilled = true;
  
  if (_trx->state()->isCoordinator()) {
    this->cleanupPlanAndEngine(TRI_ERROR_QUERY_KILLED, /*sync*/false);
  }
}
  
/// @brief return the start time of the query (steady clock value)
double Query::startTime() const noexcept { return _startTime; }

void Query::prepareQuery(SerializationFormat format) {
  try {
    init(/*createProfile*/ true);
    
    enterState(QueryExecutionState::ValueType::PARSING);

    std::unique_ptr<ExecutionPlan> plan = preparePlan();
    TRI_ASSERT(plan != nullptr);
    plan->findVarUsage();

    TRI_ASSERT(_trx != nullptr);
    TRI_ASSERT(_trx->status() == transaction::Status::RUNNING);
    
    // keep serialized copy of unchanged plan to include in query profile
    // necessary because instantiate / execution replace vars and blocks
    bool const keepPlan = _queryOptions.profile >= ProfileLevel::Blocks &&
    ServerState::isSingleServerOrCoordinator(_trx->state()->serverRole());
    if (keepPlan) {
      _planSliceCopy = std::make_unique<VPackBufferUInt8>();
      VPackBuilder b(*_planSliceCopy);
      plan->toVelocyPack(b, _ast.get(), false, ExplainRegisterPlan::No);
    }

    // simon: assumption is _queryString is empty for DBServer snippets
    bool const planRegisters = !_queryString.empty();
    ExecutionEngine::instantiateFromPlan(*this, *plan, planRegisters, format);

    _plans.push_back(std::move(plan));

    if (_snippets.size() > 1) {  // register coordinator snippets
      TRI_ASSERT(_trx->state()->isCoordinator());
      QueryRegistry* registry = QueryRegistryFeature::registry();
      if (!registry) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_SHUTTING_DOWN, "query registry not available");
      }
      registry->registerSnippets(_snippets);
    }
    
    if (_queryProfile) {
      _queryProfile->registerInQueryList();
    }
    
    enterState(QueryExecutionState::ValueType::EXECUTION);
  } catch (arangodb::basics::Exception const& ex) {
    _resultCode = ex.code();
    throw;
  } catch (std::bad_alloc const&) {
    _resultCode = TRI_ERROR_OUT_OF_MEMORY;
    throw;
  } catch (...) {
    _resultCode = TRI_ERROR_INTERNAL;
    throw;
  }
}

/// @brief prepare an AQL query, this is a preparation for execute, but
/// execute calls it internally. The purpose of this separate method is
/// to be able to only prepare a query from VelocyPack and then store it in the
/// QueryRegistry.
std::unique_ptr<ExecutionPlan> Query::preparePlan() {
  TRI_ASSERT(!_queryString.empty());
  LOG_TOPIC("9625e", DEBUG, Logger::QUERIES) << elapsedSince(_startTime)
                                             << " Query::prepare"
                                             << " this: " << (uintptr_t)this;

  TRI_ASSERT(_ast != nullptr);
  Parser parser(*this, *_ast, _queryString);
  parser.parse();

  // put in bind parameters
  parser.ast()->injectBindParameters(_bindParameters, this->resolver());

  TRI_ASSERT(_trx == nullptr);
  // needs to be created after the AST collected all collections
  std::unordered_set<std::string> inaccessibleCollections;
#ifdef USE_ENTERPRISE
  if (_queryOptions.transactionOptions.skipInaccessibleCollections) {
    inaccessibleCollections = _queryOptions.inaccessibleCollections;
  }
#endif

  _trx = AqlTransaction::create(_transactionContext, _collections,
                                _queryOptions.transactionOptions,
                                std::move(inaccessibleCollections));
  // create the transaction object, but do not start it yet
  _trx->addHint(transaction::Hints::Hint::FROM_TOPLEVEL_AQL);  // only used on toplevel

  // As soon as we start to instantiate the plan we have to clean it
  // up before killing the unique_ptr

  // we have an AST, optimize the ast
  enterState(QueryExecutionState::ValueType::AST_OPTIMIZATION);

  _ast->validateAndOptimize(*_trx);

  enterState(QueryExecutionState::ValueType::LOADING_COLLECTIONS);

  Result res = _trx->begin();

  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }
  TRI_ASSERT(_trx->status() == transaction::Status::RUNNING);

  enterState(QueryExecutionState::ValueType::PLAN_INSTANTIATION);

  auto plan = ExecutionPlan::instantiateFromAst(_ast.get(), true);

  // Run the query optimizer:
  enterState(QueryExecutionState::ValueType::PLAN_OPTIMIZATION);
  arangodb::aql::Optimizer opt(_queryOptions.maxNumberOfPlans);
  // get enabled/disabled rules
  opt.createPlans(std::move(plan), _queryOptions, false);
  // Now plan and all derived plans belong to the optimizer
  plan = opt.stealBest();  // Now we own the best one again

  TRI_ASSERT(plan != nullptr);

  // return the V8 context if we are in one
  exitV8Context();

  return plan;
}

/// @brief execute an AQL query
ExecutionState Query::execute(QueryResult& queryResult) {
  LOG_TOPIC("e8ed7", DEBUG, Logger::QUERIES) << elapsedSince(_startTime)
                                             << " Query::execute"
                                             << " this: " << (uintptr_t)this;
    
  try {
    if (killed()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_KILLED);
    }

    bool useQueryCache = canUseQueryCache();

    switch (_executionPhase) {
      case ExecutionPhase::INITIALIZE: {
        if (useQueryCache) {
          // check the query cache for an existing result
          auto cacheEntry =
              arangodb::aql::QueryCache::instance()->lookup(&_vocbase, hash(), _queryString,
                                                            bindParameters());

          if (cacheEntry != nullptr) {
            if (cacheEntry->currentUserHasPermissions()) {
              // we don't have yet a transaction when we're here, so let's
              // create a mimimal context to build the result
              queryResult.context = transaction::StandaloneContext::Create(_vocbase);
              TRI_ASSERT(cacheEntry->_queryResult != nullptr);
              queryResult.data = cacheEntry->_queryResult;
              queryResult.extra = cacheEntry->_stats;
              queryResult.cached = true;
              return ExecutionState::DONE;
            }
            // if no permissions, fall through to regular querying
          }
        }

        // will throw if it fails
        if (!_ast) { // simon: hack for AQL_EXECUTEJSON
          prepareQuery(SerializationFormat::SHADOWROWS);
        }

        log();
        // NOTE: If the options have a shorter lifetime than the builder, it
        // gets invalid (at least set() and close() are broken).
        queryResult.data = std::make_shared<VPackBuilder>(&vpackOptions());

        // reserve some space in Builder to avoid frequent reallocs
        queryResult.data->reserve(16 * 1024);
        queryResult.data->openArray(/*unindexed*/true);
        
        _executionPhase = ExecutionPhase::EXECUTE;
      }
      [[fallthrough]];
      case ExecutionPhase::EXECUTE: {
        TRI_ASSERT(queryResult.data != nullptr);
        TRI_ASSERT(queryResult.data->isOpenArray());
        TRI_ASSERT(_trx != nullptr);

        if (useQueryCache && (isModificationQuery()  || !_warnings.empty() ||
                              !_ast->root()->isCacheable())) {
          useQueryCache = false;
        }

        ExecutionEngine* engine = this->rootEngine();
        TRI_ASSERT(engine != nullptr);

        // this is the RegisterId our results can be found in
        RegisterId const resultRegister = engine->resultRegister();

        // We loop as long as we are having ExecState::DONE returned
        // In case of WAITING we return, this function is repeatable!
        // In case of HASMORE we loop
        while (true) {
          AqlCallStack defaultStack{AqlCallList{AqlCall{}}};
          auto const& [state, skipped, block] = engine->execute(defaultStack);
          // The default call asks for No skips.
          TRI_ASSERT(skipped.nothingSkipped());
          if (state == ExecutionState::WAITING) {
            return state;
          }

          // block == nullptr => state == DONE
          if (block == nullptr) {
            TRI_ASSERT(state == ExecutionState::DONE);
            break;
          }

          if (!_queryOptions.silent && resultRegister.isValid()) {
            // cache low-level pointer to avoid repeated shared-ptr-derefs
            TRI_ASSERT(queryResult.data != nullptr);
            auto& resultBuilder = *queryResult.data;
            size_t previousLength = resultBuilder.bufferRef().byteSize();
            auto& vpackOpts = vpackOptions();

            size_t const n = block->numRows();

            for (size_t i = 0; i < n; ++i) {
              AqlValue const& val = block->getValueReference(i, resultRegister);

              if (!val.isEmpty()) {
                val.toVelocyPack(&vpackOpts, resultBuilder,
                                 /*resolveExternals*/useQueryCache,
                                 /*allowUnindexed*/true);
              }
            }

            size_t newLength = resultBuilder.bufferRef().byteSize();
            TRI_ASSERT(newLength >= previousLength);
            size_t diff = newLength - previousLength;

            _resourceMonitor.increaseMemoryUsage(diff);
            _resultMemoryUsage += diff;
          }

          if (state == ExecutionState::DONE) {
            break;
          }
        }

        // must close result array here because it must be passed as a closed
        // array to the query cache
        queryResult.data->close();

        if (useQueryCache && _warnings.empty()) {
          std::unordered_map<std::string, std::string> dataSources = _queryDataSources;

          _trx->state()->allCollections(  // collect transaction DataSources
              [&dataSources](TransactionCollection& trxCollection) -> bool {
                auto const& c = trxCollection.collection();
                dataSources.try_emplace(c->guid(), c->name());
                return true;  // continue traversal
              });

          // create a query cache entry for later storage
          _cacheEntry =
              std::make_unique<QueryCacheResultEntry>(hash(), _queryString, queryResult.data,
                                                      bindParameters(),
                                                      std::move(dataSources)  // query DataSources
              );
        }

        queryResult.context = _trx->transactionContext();
        _executionPhase = ExecutionPhase::FINALIZE;
      }

      [[fallthrough]];
      case ExecutionPhase::FINALIZE: {
        if (!queryResult.extra) {
          queryResult.extra = std::make_shared<VPackBuilder>();
        }
        // will set warnings, stats, profile and cleanup plan and engine
        auto state = finalize(*queryResult.extra);
        if (state == ExecutionState::DONE && _cacheEntry != nullptr) {
          _cacheEntry->_stats = queryResult.extra;
          QueryCache::instance()->store(&_vocbase, std::move(_cacheEntry));
        }
        return state;
      }
    }
    // We should not be able to get here
    TRI_ASSERT(false);
  } catch (arangodb::basics::Exception const& ex) {
    queryResult.reset(Result(ex.code(), "AQL: " + ex.message() +
                                            QueryExecutionState::toStringWithPrefix(_execState)));
    cleanupPlanAndEngine(ex.code(), /*sync*/true);
  } catch (std::bad_alloc const&) {
    queryResult.reset(
        Result(TRI_ERROR_OUT_OF_MEMORY,
               StringUtils::concatT(TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY),
                                    QueryExecutionState::toStringWithPrefix(_execState))));
    cleanupPlanAndEngine(TRI_ERROR_OUT_OF_MEMORY, /*sync*/true);
  } catch (std::exception const& ex) {
    queryResult.reset(Result(TRI_ERROR_INTERNAL,
                             ex.what() + QueryExecutionState::toStringWithPrefix(_execState)));
    cleanupPlanAndEngine(TRI_ERROR_INTERNAL, /*sync*/true);
  } catch (...) {
    queryResult.reset(
        Result(TRI_ERROR_INTERNAL,
               StringUtils::concatT(TRI_errno_string(TRI_ERROR_INTERNAL),
                                    QueryExecutionState::toStringWithPrefix(_execState))));
    cleanupPlanAndEngine(TRI_ERROR_INTERNAL, /*sync*/true);
  }
  
  return ExecutionState::DONE;
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
QueryResult Query::executeSync() {
  std::shared_ptr<SharedQueryState> ss;

  QueryResult queryResult;
  do {
    auto state = execute(queryResult);
    if (state != aql::ExecutionState::WAITING) {
      TRI_ASSERT(state == aql::ExecutionState::DONE);
      return queryResult;
    }

    if (!ss) {
      ss = sharedState();
    }

    TRI_ASSERT(ss != nullptr);
    ss->waitForAsyncWakeup();
  } while (true);
}

// execute an AQL query: may only be called with an active V8 handle scope
QueryResultV8 Query::executeV8(v8::Isolate* isolate) {
  LOG_TOPIC("6cac7", DEBUG, Logger::QUERIES) << elapsedSince(_startTime)
                                             << " Query::executeV8"
                                             << " this: " << (uintptr_t)this;

  aql::QueryResultV8 queryResult;

  try {
    bool useQueryCache = canUseQueryCache();

    if (useQueryCache) {
      // check the query cache for an existing result
      auto cacheEntry =
          arangodb::aql::QueryCache::instance()->lookup(&_vocbase, hash(), _queryString,
                                                        bindParameters());

      if (cacheEntry != nullptr) {
        if (cacheEntry->currentUserHasPermissions()) {
          // we don't have yet a transaction when we're here, so let's create
          // a mimimal context to build the result
          queryResult.context = transaction::StandaloneContext::Create(_vocbase);
          v8::Handle<v8::Value> values =
              TRI_VPackToV8(isolate, cacheEntry->_queryResult->slice(),
                            queryResult.context->getVPackOptions());
          TRI_ASSERT(values->IsArray());
          queryResult.v8Data = v8::Handle<v8::Array>::Cast(values);
          queryResult.extra = cacheEntry->_stats;
          queryResult.cached = true;
          return queryResult;
        }
        // if no permissions, fall through to regular querying
      }
    }

    // will throw if it fails
    prepareQuery(SerializationFormat::SHADOWROWS);

    log();

    if (useQueryCache && (isModificationQuery()  || !_warnings.empty() ||
                          !_ast->root()->isCacheable())) {
      useQueryCache = false;
    }

    v8::Handle<v8::Array> resArray = v8::Array::New(isolate);

    auto* engine = this->rootEngine();
    TRI_ASSERT(engine != nullptr);

    std::shared_ptr<SharedQueryState> ss = engine->sharedState();
    TRI_ASSERT(ss != nullptr);

    // this is the RegisterId our results can be found in
    auto const resultRegister = engine->resultRegister();

    // following options and builder only required for query cache
    VPackOptions options = VPackOptions::Defaults;
    options.buildUnindexedArrays = true;
    options.buildUnindexedObjects = true;
    auto builder = std::make_shared<VPackBuilder>(&options);

    try {
      ss->resetWakeupHandler();

      // iterate over result, return it and optionally store it in query cache
      builder->openArray();

      // iterate over result and return it
      uint32_t j = 0;
      ExecutionState state = ExecutionState::HASMORE;
      auto context = TRI_IGETC;
      while (state != ExecutionState::DONE) {
        auto res = engine->getSome(ExecutionBlock::DefaultBatchSize);
        state = res.first;
        while (state == ExecutionState::WAITING) {
          ss->waitForAsyncWakeup();
          res = engine->getSome(ExecutionBlock::DefaultBatchSize);
          state = res.first;
        }
        SharedAqlItemBlockPtr value = std::move(res.second);

        // value == nullptr => state == DONE
        TRI_ASSERT(value != nullptr || state == ExecutionState::DONE);

        if (value == nullptr) {
          continue;
        }

        if (!_queryOptions.silent && resultRegister.isValid()) {
          size_t memoryUsage = 0;
          size_t const n = value->numRows();

          auto const& vpackOpts = vpackOptions();
          for (size_t i = 0; i < n; ++i) {
            AqlValue const& val = value->getValueReference(i, resultRegister);

            if (!val.isEmpty()) {
              resArray->Set(context, j++, val.toV8(isolate, &vpackOptions())).FromMaybe(false);

              if (useQueryCache) {
                val.toVelocyPack(&vpackOpts, *builder,
                                 /*resolveExternals*/true,
                                 /*allowUnindexed*/true);
              }
              memoryUsage += sizeof(v8::Value);
              if (val.requiresDestruction()){
                memoryUsage += val.memoryUsage();
              }

              if (V8PlatformFeature::isOutOfMemory(isolate)) {
                THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
              }
            }
          }

          // this may throw
          _resourceMonitor.increaseMemoryUsage(memoryUsage);
          _resultMemoryUsage += memoryUsage;
        }

        if (killed()) {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_KILLED);
        }
      }

      builder->close();
    } catch (...) {
      LOG_TOPIC("8a6bf", DEBUG, Logger::QUERIES)
          << elapsedSince(_startTime)
          << " got an exception executing "
          << " this: " << (uintptr_t)this;
      throw;
    }

    queryResult.v8Data = resArray;
    queryResult.context = _trx->transactionContext();
    queryResult.extra = std::make_shared<VPackBuilder>();

    if (useQueryCache && _warnings.empty()) {
      auto dataSources = _queryDataSources;

      _trx->state()->allCollections(  // collect transaction DataSources
          [&dataSources](TransactionCollection& trxCollection) -> bool {
            auto const& c = trxCollection.collection();
            dataSources.try_emplace(c->guid(), c->name());
            return true;  // continue traversal
          });

      // create a cache entry for later usage
      _cacheEntry = std::make_unique<QueryCacheResultEntry>(hash(), _queryString,
                                                            builder, bindParameters(),
                                                            std::move(dataSources)  // query DataSources
      );
    }

    ss->resetWakeupHandler();
    
    // will set warnings, stats, profile and cleanup plan and engine
    ExecutionState state = finalize(*queryResult.extra);
    while (state == ExecutionState::WAITING) {
      ss->waitForAsyncWakeup();
      state = finalize(*queryResult.extra);
    }
    if (_cacheEntry != nullptr) {
      _cacheEntry->_stats = queryResult.extra;
      QueryCache::instance()->store(&_vocbase, std::move(_cacheEntry));
    }
    // fallthrough to returning queryResult below...
  } catch (arangodb::basics::Exception const& ex) {
    queryResult.reset(Result(ex.code(), "AQL: " + ex.message() +
                                            QueryExecutionState::toStringWithPrefix(_execState)));
    cleanupPlanAndEngine(ex.code(), /*sync*/true);
  } catch (std::bad_alloc const&) {
    queryResult.reset(
        Result(TRI_ERROR_OUT_OF_MEMORY,
               StringUtils::concatT(TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY),
                                    QueryExecutionState::toStringWithPrefix(_execState))));
    cleanupPlanAndEngine(TRI_ERROR_OUT_OF_MEMORY, /*sync*/true);
  } catch (std::exception const& ex) {
    queryResult.reset(Result(TRI_ERROR_INTERNAL,
                             ex.what() + QueryExecutionState::toStringWithPrefix(_execState)));
    cleanupPlanAndEngine(TRI_ERROR_INTERNAL, /*sync*/true);
  } catch (...) {
    queryResult.reset(
        Result(TRI_ERROR_INTERNAL,
               StringUtils::concatT(TRI_errno_string(TRI_ERROR_INTERNAL),
                                    QueryExecutionState::toStringWithPrefix(_execState))));
    cleanupPlanAndEngine(TRI_ERROR_INTERNAL, /*sync*/ true);
  }

  return queryResult;
}

ExecutionState Query::finalize(VPackBuilder& extras) {
  if (_queryProfile != nullptr &&
      _shutdownState.load(std::memory_order_relaxed) == ShutdownState::None) {
    // the following call removes the query from the list of currently
    // running queries. so whoever fetches that list will not see a Query that
    // is about to shut down/be destroyed
    _queryProfile->unregisterFromQueryList();
  }
  
  auto state = cleanupPlanAndEngine(TRI_ERROR_NO_ERROR, /*sync*/false);
  if (state == ExecutionState::WAITING) {
    return state;
  }
  
  extras.openObject(/*unindexed*/true);
  _warnings.toVelocyPack(extras);
  
  if (!_snippets.empty()) {
    _execStats.requests += _numRequests.load(std::memory_order_relaxed);
    _execStats.setPeakMemoryUsage(_resourceMonitor.peak());
    _execStats.setExecutionTime(elapsedSince(_startTime));
    for (auto& engine : _snippets) {
      engine->collectExecutionStats(_execStats);
    }

    extras.add(VPackValue("stats"));
    _execStats.toVelocyPack(extras, _queryOptions.fullCount);
    
    if (_planSliceCopy) {
      extras.add("plan", VPackSlice(_planSliceCopy->data()));
    }
  }

  double now = currentSteadyClockValue();
  if (_queryProfile != nullptr && _queryOptions.profile >= ProfileLevel::Basic) {
    _queryProfile->setStateEnd(QueryExecutionState::ValueType::FINALIZATION, now);
    _queryProfile->toVelocyPack(extras);
  }
  extras.close();

  LOG_TOPIC("95996", DEBUG, Logger::QUERIES) << now - _startTime
                                             << " Query::finalize:returning"
                                             << " this: " << (uintptr_t)this;
  return ExecutionState::DONE;
}

/// @brief parse an AQL query
QueryResult Query::parse() {
  // only used in case case of failure
  QueryResult result;

  try {
    init(/*createProfile*/ false);
    Parser parser(*this, *_ast, _queryString);
    return parser.parseWithDetails();

  } catch (arangodb::basics::Exception const& ex) {
    result.reset(Result(ex.code(), ex.message()));
  } catch (std::bad_alloc const&) {
    result.reset(Result(TRI_ERROR_OUT_OF_MEMORY));
  } catch (std::exception const& ex) {
    result.reset(Result(TRI_ERROR_INTERNAL, ex.what()));
  } catch (...) {
    result.reset(Result(TRI_ERROR_INTERNAL, "an unknown error occurred while parsing the query"));
  }

  TRI_ASSERT(result.fail());
  return result;
}

/// @brief explain an AQL query
QueryResult Query::explain() {
  QueryResult result;

  try {
    init(/*createProfile*/ false);
    enterState(QueryExecutionState::ValueType::PARSING);

    Parser parser(*this, *_ast, _queryString);
    parser.parse();

    // put in bind parameters
    parser.ast()->injectBindParameters(_bindParameters, this->resolver());

    // optimize and validate the ast
    enterState(QueryExecutionState::ValueType::AST_OPTIMIZATION);

    // create the transaction object, but do not start it yet
    _trx = AqlTransaction::create(_transactionContext, _collections,
                                  _queryOptions.transactionOptions);

    // we have an AST
    Result res = _trx->begin();

    if (!res.ok()) {
      THROW_ARANGO_EXCEPTION(res);
    }

    enterState(QueryExecutionState::ValueType::LOADING_COLLECTIONS);
    _ast->validateAndOptimize(*_trx);

    enterState(QueryExecutionState::ValueType::PLAN_INSTANTIATION);
    std::unique_ptr<ExecutionPlan> plan =
        ExecutionPlan::instantiateFromAst(parser.ast(), true);

    // Run the query optimizer:
    enterState(QueryExecutionState::ValueType::PLAN_OPTIMIZATION);
    arangodb::aql::Optimizer opt(_queryOptions.maxNumberOfPlans);
    // get enabled/disabled rules
    opt.createPlans(std::move(plan), _queryOptions, true);

    enterState(QueryExecutionState::ValueType::FINALIZATION);

    auto preparePlanForSerialization = [&](std::unique_ptr<ExecutionPlan> const& plan) {
      plan->findVarUsage();
      plan->planRegisters(_queryOptions.explainRegisters);
      plan->findCollectionAccessVariables();
      plan->prepareTraversalOptions();
    };

    if (_queryOptions.allPlans) {
      result.data = std::make_shared<VPackBuilder>();
      {
        VPackArrayBuilder guard(result.data.get());

        auto const& plans = opt.getPlans();
        for (auto& it : plans) {
          auto& pln = it.first;
          TRI_ASSERT(pln != nullptr);

          preparePlanForSerialization(pln);
          pln->toVelocyPack(*result.data.get(), parser.ast(), _queryOptions.verbosePlans,
                            _queryOptions.explainRegisters);
        }
      }
      // cacheability not available here
      result.cached = false;
    } else {
      // Now plan and all derived plans belong to the optimizer
      std::unique_ptr<ExecutionPlan> bestPlan = opt.stealBest();  // Now we own the best one again
      TRI_ASSERT(bestPlan != nullptr);

      preparePlanForSerialization(bestPlan);
      result.data = bestPlan->toVelocyPack(parser.ast(), _queryOptions.verbosePlans,
                                           _queryOptions.explainRegisters);

      // cacheability
      result.cached = (!_queryString.empty() && !isModificationQuery() &&
                       _warnings.empty() && _ast->root()->isCacheable());
    }

    // technically no need to commit, as we are only explaining here
    auto commitResult = _trx->commit();
    if (commitResult.fail()) {
      THROW_ARANGO_EXCEPTION(commitResult);
    }

    result.extra = std::make_shared<VPackBuilder>();
    {
      VPackObjectBuilder guard(result.extra.get(), /*unindexed*/true);
      _warnings.toVelocyPack(*result.extra);
      result.extra->add(VPackValue("stats"));
      opt._stats.toVelocyPack(*result.extra);
    }

  } catch (arangodb::basics::Exception const& ex) {
    result.reset(Result(ex.code(), ex.message() + QueryExecutionState::toStringWithPrefix(_execState)));
  } catch (std::bad_alloc const&) {
    result.reset(
        Result(TRI_ERROR_OUT_OF_MEMORY,
               StringUtils::concatT(TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY),
                                    QueryExecutionState::toStringWithPrefix(_execState))));
  } catch (std::exception const& ex) {
    result.reset(Result(TRI_ERROR_INTERNAL, ex.what() + QueryExecutionState::toStringWithPrefix(_execState)));
  } catch (...) {
    result.reset(Result(TRI_ERROR_INTERNAL,
                        StringUtils::concatT(TRI_errno_string(TRI_ERROR_INTERNAL),
                                             QueryExecutionState::toStringWithPrefix(_execState))));
  }

  // will be returned in success or failure case
  return result;
}

bool Query::isModificationQuery() const noexcept {
  TRI_ASSERT(_ast != nullptr);
  return _ast->containsModificationNode();
}

bool Query::isAsyncQuery() const noexcept {
  TRI_ASSERT(_ast != nullptr);
  return _ast->canApplyParallelism();
}

/// @brief enter a V8 context
void Query::enterV8Context() {
  auto registerCtx = [&] {
    // register transaction in context
    if (_transactionContext->isV8Context()) {
      auto ctx = static_cast<arangodb::transaction::V8Context*>(_transactionContext.get());
      ctx->enterV8Context();
    } else {
      v8::Isolate* isolate = v8::Isolate::GetCurrent();
      TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(isolate->GetData(arangodb::V8PlatformFeature::V8_DATA_SLOT));
      v8g->_transactionState = _trx->stateShrdPtr();
    }
  };
  
  if (!_contextOwnedByExterior) {
    if (_v8Context == nullptr) {
      auto& server = vocbase().server();
      if (!server.hasFeature<V8DealerFeature>() || !server.isEnabled<V8DealerFeature>()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                       "V8 engine is disabled");
      }
      JavaScriptSecurityContext securityContext =
          JavaScriptSecurityContext::createQueryContext();
      _v8Context = server.getFeature<V8DealerFeature>().enterContext(&_vocbase, securityContext);

      if (_v8Context == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_RESOURCE_LIMIT,
            "unable to enter V8 context for query execution");
      }
      registerCtx();
    }
    TRI_ASSERT(_v8Context != nullptr);
  } else if (!_embeddedQuery && !_registeredInV8Context) {  // may happen for stream trx
    registerCtx();
    _registeredInV8Context = true;
  }
}

/// @brief return a V8 context
void Query::exitV8Context() {
  auto unregister = [&] {
    if (_transactionContext->isV8Context()) {  // necessary for stream trx
      auto ctx = static_cast<arangodb::transaction::V8Context*>(_transactionContext.get());
      ctx->exitV8Context();
    } else {
      v8::Isolate* isolate = v8::Isolate::GetCurrent();
      TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(isolate->GetData(arangodb::V8PlatformFeature::V8_DATA_SLOT));
      v8g->_transactionState = nullptr;
    }
  };
  if (!_contextOwnedByExterior) {
    if (_v8Context != nullptr) {
      // unregister transaction in context
      unregister();

      auto& server = vocbase().server();
      TRI_ASSERT(server.hasFeature<V8DealerFeature>() &&
                 server.isEnabled<V8DealerFeature>());
      server.getFeature<V8DealerFeature>().exitContext(_v8Context);
      _v8Context = nullptr;
    }
  } else if (!_embeddedQuery && _registeredInV8Context) {
    // prevent duplicate deregistration
    unregister();
    _registeredInV8Context = false;
  }
}

/// @brief initializes the query
void Query::init(bool createProfile) {
  TRI_ASSERT(!_queryProfile && !_ast);
  if (_queryProfile || _ast) {
    // already called
    return;
  }

  TRI_ASSERT(_queryProfile == nullptr);
  // adds query to QueryList which is needed for /_api/query/current
  if (createProfile && !ServerState::instance()->isDBServer()) {
    _queryProfile = std::make_unique<QueryProfile>(this);
  }
  enterState(QueryExecutionState::ValueType::INITIALIZATION);

  TRI_ASSERT(_ast == nullptr);
  _ast = std::make_unique<Ast>(*this);
}

/// @brief calculate a hash for the query, once
uint64_t Query::hash() {
  if (!_queryHashCalculated) {
    _queryHash = calculateHash();
    _queryHashCalculated = true;
  }
  return _queryHash;
}

/// @brief log a query
void Query::log() {
  if (!_queryString.empty()) {
    LOG_TOPIC("8a86a", TRACE, Logger::QUERIES)
        << "executing query " << _queryId << ": '" << _queryString.extract(1024) << "'";
  }
}

/// @brief calculate a hash value for the query and bind parameters
uint64_t Query::calculateHash() const {
  if (_queryString.empty()) {
    return DontCache;
  }

  // hash the query string first
  uint64_t hashval = _queryString.hash();

  // handle "fullCount" option. if this option is set, the query result will
  // be different to when it is not set!
  if (_queryOptions.fullCount) {
    hashval = fasthash64(TRI_CHAR_LENGTH_PAIR("fullcount:true"), hashval);
  } else {
    hashval = fasthash64(TRI_CHAR_LENGTH_PAIR("fullcount:false"), hashval);
  }

  // handle "count" option
  if (_queryOptions.count) {
    hashval = fasthash64(TRI_CHAR_LENGTH_PAIR("count:true"), hashval);
  } else {
    hashval = fasthash64(TRI_CHAR_LENGTH_PAIR("count:false"), hashval);
  }

  // also hash "optimizer.rules" options
  for (auto const& rule : _queryOptions.optimizerRules) {
    hashval = VELOCYPACK_HASH(rule.data(), rule.size(), hashval);
  }
  // blend query hash with bind parameters
  return hashval ^ _bindParameters.hash();
}

/// @brief whether or not the query cache can be used for the query
bool Query::canUseQueryCache() const {
  if (_queryString.size() < 8 || _queryOptions.silent) {
    return false;
  }

  auto queryCacheMode = QueryCache::instance()->mode();

  if (_queryOptions.cache &&
      (queryCacheMode == CACHE_ALWAYS_ON || queryCacheMode == CACHE_ON_DEMAND)) {
    // cache mode is set to always on or on-demand...
    // query will only be cached if `cache` attribute is not set to false

    // cannot use query cache on a coordinator at the moment
    return !arangodb::ServerState::instance()->isRunningInCluster();
  }

  return false;
}

ErrorCode Query::resultCode() const noexcept {
  // never return negative value from here
  return _resultCode.value_or(TRI_ERROR_NO_ERROR);
}

/// @brief enter a new state
void Query::enterState(QueryExecutionState::ValueType state) {
  LOG_TOPIC("d8767", DEBUG, Logger::QUERIES)
      << elapsedSince(_startTime)
      << " Query::enterState: " << arangodb::aql::QueryExecutionState::toString(state) << " this: " << (uintptr_t)this;
  if (_queryProfile != nullptr) {
    // record timing for previous state
    _queryProfile->setStateDone(_execState);
  }

  // and adjust the state
  _execState = state;
}

/// @brief cleanup plan and engine for current query
ExecutionState Query::cleanupPlanAndEngine(ErrorCode errorCode, bool sync) {
  if (!_resultCode.has_value()) {
    // result code not yet set.
    _resultCode = errorCode;
  }

  if (sync && _sharedState) {
    _sharedState->resetWakeupHandler();
    auto state = cleanupTrxAndEngines(errorCode);
    while (state == ExecutionState::WAITING) {
      _sharedState->waitForAsyncWakeup();
      state = cleanupTrxAndEngines(errorCode);
    }
  }
  
  return cleanupTrxAndEngines(errorCode);
}

void Query::unregisterSnippets() {
  if (!_snippets.empty() && ServerState::instance()->isCoordinator()) {
    auto* registry = QueryRegistryFeature::registry();
    if (registry) {
      registry->unregisterSnippets(_snippets);
    }
  }
}

/// @brief pass-thru a resolver object from the transaction context
CollectionNameResolver const& Query::resolver() const {
  return _transactionContext->resolver();
}

/// @brief create a transaction::Context
std::shared_ptr<transaction::Context> Query::newTrxContext() const {

  TRI_ASSERT(_transactionContext != nullptr);
  TRI_ASSERT(_trx != nullptr);

  if (_ast->canApplyParallelism()) {
    TRI_ASSERT(!_ast->containsModificationNode());
    return _transactionContext->clone();
  }
  return _transactionContext;
}

velocypack::Options const& Query::vpackOptions() const {
  return *(_transactionContext->getVPackOptions());
}

transaction::Methods& Query::trxForOptimization() {
  TRI_ASSERT(_execState != QueryExecutionState::ValueType::EXECUTION);
  return *_trx;
}

#ifdef ARANGODB_USE_GOOGLE_TESTS
void Query::initForTests() {
  this->init(/*createProfile*/ false);
  initTrxForTests();
}

void Query::initTrxForTests() {
  _trx = AqlTransaction::create(_transactionContext, _collections, _queryOptions.transactionOptions,
                                std::unordered_set<std::string>{});
  // create the transaction object, but do not start it yet
  _trx->addHint(transaction::Hints::Hint::FROM_TOPLEVEL_AQL);  // only used on toplevel
  auto res = _trx->begin();
  TRI_ASSERT(res.ok());
}
#endif

/// @brief return the query's shared state
std::shared_ptr<SharedQueryState> Query::sharedState() const {
  return _sharedState;
}

ExecutionEngine* Query::rootEngine() const {
  if (!_snippets.empty()) {
    TRI_ASSERT(ServerState::instance()->isDBServer() ||
               _snippets[0]->engineId() == 0);
    return _snippets[0].get();
  }
  return nullptr;
}

namespace {

futures::Future<Result> finishDBServerParts(Query& query, ErrorCode errorCode) {
  TRI_ASSERT(ServerState::instance()->isCoordinator());
  auto& serverQueryIds = query.serverQueryIds();
  TRI_ASSERT(!serverQueryIds.empty());
  
  NetworkFeature const& nf = query.vocbase().server().getFeature<NetworkFeature>();
  network::ConnectionPool* pool = nf.pool();
  if (pool == nullptr) {
    return futures::makeFuture(Result{TRI_ERROR_SHUTTING_DOWN});
  }
  
  network::RequestOptions options;
  options.database = query.vocbase().name();
  options.timeout = network::Timeout(60.0);  // Picked arbitrarily
  options.continuationLane = RequestLane::CLUSTER_AQL_CONTINUATION;
  //  options.skipScheduler = true;

  VPackBuffer<uint8_t> body;
  VPackBuilder builder(body);
  builder.openObject(true);
  builder.add(StaticStrings::Code, VPackValue(errorCode));
  builder.close();
  
  query.incHttpRequests(static_cast<unsigned>(serverQueryIds.size()));
   
  std::vector<futures::Future<Result>> futures;
  futures.reserve(serverQueryIds.size());
  auto ss = query.sharedState();
  TRI_ASSERT(ss != nullptr);

  for (auto const& [serverDst, queryId] : serverQueryIds) {

    TRI_ASSERT(serverDst.substr(0, 7) == "server:");
    
    auto f = network::sendRequest(pool, serverDst, fuerte::RestVerb::Delete,
                         "/_api/aql/finish/" + std::to_string(queryId), body, options)
    .thenValue([ss, &query](network::Response&& res) mutable -> Result {
        // simon: checked until 3.5, shutdown result is always ignored
        if (res.fail()) {
          return Result{network::fuerteToArangoErrorCode(res)};
        } else if (!res.slice().isObject()) {
          return Result(TRI_ERROR_CLUSTER_AQL_COMMUNICATION,
                        "shutdown response of DBServer is malformed");
        }
        
        VPackSlice val = res.slice().get("stats");
        if (val.isObject()) {
          ss->executeLocked([&] {
            query.executionStats().add(ExecutionStats(val));
          });
        }
        // read "warnings" attribute if present and add it to our query
        val = res.slice().get("warnings");
        if (val.isArray()) {
          for (VPackSlice it : VPackArrayIterator(val)) {
            if (it.isObject()) {
              VPackSlice code = it.get("code");
              VPackSlice message = it.get("message");
              if (code.isNumber() && message.isString()) {
                query.warnings().registerWarning(ErrorCode{code.getNumericValue<int>()},
                                                 message.copyString());
              }
            }
          }
        }

        val = res.slice().get("code");
        if (val.isNumber()) {
          return Result{ErrorCode{val.getNumericValue<int>()}};
        }
        return Result();
    }).thenError<std::exception>([](std::exception ptr) {
      return Result(TRI_ERROR_INTERNAL, "unhandled query shutdown exception");
    });

    futures.emplace_back(std::move(f));
  }
  
  return futures::collectAll(std::move(futures))
         .thenValue([](std::vector<futures::Try<Result>>&& results) -> Result{
           for (futures::Try<Result>& tryRes : results) {
             if (tryRes.get().fail()) {
               return std::move(tryRes).get();
             }
           }
           return Result();
  });
}
} // namespace

aql::ExecutionState Query::cleanupTrxAndEngines(ErrorCode errorCode) {
  ShutdownState exp = _shutdownState.load(std::memory_order_relaxed);
  if (exp == ShutdownState::Done) {
    return ExecutionState::DONE;
  } else if (exp == ShutdownState::InProgress) {
    return ExecutionState::WAITING;
  }

  TRI_ASSERT (exp == ShutdownState::None);
  if (!_shutdownState.compare_exchange_strong(exp, ShutdownState::InProgress,
                                              std::memory_order_relaxed)) {
    return ExecutionState::WAITING; // someone else got here
  }
  
  enterState(QueryExecutionState::ValueType::FINALIZATION);
  
  // simon: do not unregister _queryProfile here, since kill() will be called
  //        under the same QueryList lock
  
  // The above condition is not true if we have already waited.
  LOG_TOPIC("fc22c", DEBUG, Logger::QUERIES)
      << elapsedSince(_startTime)
      << " Query::finalize: before _trx->commit"
      << " this: " << (uintptr_t)this;

  // Only one thread is allowed to call commit
  if (errorCode == TRI_ERROR_NO_ERROR) {
    ScopeGuard guard([this](){
      // If we exit here we need to throw the error.
      // The caller will handle the error and will call this method
      // again using an errorCode != NO_ERROR.
      // If we do not reset to None here, this additional call will cause endless
      // looping.
      _shutdownState.store(ShutdownState::None, std::memory_order_relaxed);
    });
    futures::Future<Result> commitResult = _trx->commitAsync();
    TRI_ASSERT(commitResult.isReady());
    if (commitResult.get().fail()) {
      THROW_ARANGO_EXCEPTION(std::move(commitResult).get());
    }
    TRI_IF_FAILURE("Query::finalize_before_done") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
    // We succeeded with commit. Let us cancel the guard
    // The state of "in progress" is now correct if we exit the method.
    guard.cancel();
  }

  LOG_TOPIC("7ef18", DEBUG, Logger::QUERIES)
      << elapsedSince(_startTime)
      << " Query::finalize: before cleanupPlanAndEngine"
      << " this: " << (uintptr_t)this;

  if (_serverQueryIds.empty()) {
    _shutdownState.store(ShutdownState::Done, std::memory_order_relaxed);
    return ExecutionState::DONE;
  }
  
  TRI_ASSERT(ServerState::instance()->isCoordinator());
  TRI_ASSERT(_sharedState);
  try {
    TRI_IF_FAILURE("Query::finalize_error_on_finish_db_servers") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL_AQL);
    }
    ::finishDBServerParts(*this, errorCode).thenValue([ss = _sharedState, this](Result r) {
      LOG_TOPIC_IF("fd31e", INFO, Logger::QUERIES, r.fail() && r.isNot(TRI_ERROR_HTTP_NOT_FOUND))
        << "received error from DBServer on query finalization: " << r.errorNumber() << ", '" << r.errorMessage() << "'";
      _sharedState->executeAndWakeup([&] {
        _shutdownState.store(ShutdownState::Done, std::memory_order_relaxed);
        return true;
      });
    });
    return ExecutionState::WAITING;
  } catch (...) {
    // In case of any error that happened in sending out the requests
    // we simply reset to done, we tried to cleanup the engines.
    // we only get here if something in the network stack is out of order.
    // so there is no need to retry on cleaning up the engines, caller can continue
    // Also note: If an error in cleanup happens the query was completed already,
    // so this error does not need to be reported to client.
    _shutdownState.store(ShutdownState::Done, std::memory_order_relaxed);

    if (isModificationQuery()) {
      // For modification queries these left-over locks will have negative side effects
      // We will report those to the user.
      // Lingering Read-locks should not block the system.
      std::vector<std::string_view> writeLocked{};
      std::vector<std::string_view> exclusiveLocked{};
      _collections.visit([&](std::string const& name, Collection& col) -> bool {
        switch (col.accessType()) {
          case AccessMode::Type::WRITE: {
            writeLocked.emplace_back(name);
            break;
          }
          case AccessMode::Type::EXCLUSIVE: {
            exclusiveLocked.emplace_back(name);
            break;
          }
          default:
            // We do not need to capture reads.
            break;
        }
        return true;
      });
      LOG_TOPIC("63572", WARN, Logger::QUERIES)
          << " Failed to cleanup leftovers of a query due to communication errors. "
          << " The DBServers will eventually clean up the state. The following locks still exist: "
          << " write: " << writeLocked << ": you may not drop these collections until the locks time out."
          << " exclusive: " << exclusiveLocked << ": you may not be able to write into these collections until the locks time out.";
      for (auto const& [serverDst, queryId] : _serverQueryIds) {
        TRI_ASSERT(serverDst.substr(0, 7) == "server:");
        auto msg = "Failed to send unlock request DELETE /_api/aql/finish/" + std::to_string(queryId) + " to " + serverDst + " in database " + vocbase().name();
        _warnings.registerWarning(TRI_ERROR_CLUSTER_AQL_COMMUNICATION, msg);
        LOG_TOPIC("7c10f", WARN, Logger::QUERIES) << msg;
      }
    }
    return ExecutionState::DONE;
  }
}
