////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "ClusterQuery.h"

#include "Aql/Ast.h"
#include "Aql/AqlTransaction.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/Timing.h"
#include "Aql/QueryRegistry.h"
#include "Aql/QueryProfile.h"
#include "Cluster/ServerState.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Context.h"
#include "RestServer/QueryRegistryFeature.h"
#include "Cluster/TraverserEngine.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

ClusterQuery::ClusterQuery(std::shared_ptr<transaction::Context> const& ctx,
                           QueryOptions&& options)
    : Query(ctx, aql::QueryString(), /*bindParams*/ nullptr, std::move(options),
            /*sharedState*/ ServerState::instance()->isDBServer()
                ? nullptr
                : std::make_shared<SharedQueryState>(ctx->vocbase().server())) {}

ClusterQuery::~ClusterQuery() {
  try {
    _traversers.clear();
  } catch (...) {
  }
}

void ClusterQuery::prepareClusterQuery(VPackSlice querySlice,
                                       VPackSlice collections,
                                       VPackSlice variables,
                                       VPackSlice snippets,
                                       VPackSlice traverserSlice,
                                       VPackBuilder& answerBuilder,
                                       arangodb::QueryAnalyzerRevisions const& analyzersRevision) {
  LOG_TOPIC("9636f", DEBUG, Logger::QUERIES) << elapsedSince(_startTime)
                                             << " ClusterQuery::prepareClusterQuery"
                                             << " this: " << (uintptr_t)this;

  init(/*createProfile*/ true);

  VPackSlice val = querySlice.get("isModificationQuery");
  if (val.isBool() && val.getBool()) {
    _ast->setContainsModificationNode();
  }
  val = querySlice.get("isAsyncQuery");
  if (val.isBool() && val.getBool()) {
    _ast->setContainsParallelNode();
  }

  enterState(QueryExecutionState::ValueType::LOADING_COLLECTIONS);

  // FIXME change this format to take the raw one?
  ExecutionPlan::getCollectionsFromVelocyPack(_collections, collections);
  _ast->variables()->fromVelocyPack(variables);
  // creating the plan may have produced some collections
  // we need to add them to the transaction now (otherwise the query will fail)

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
  if (_trx->state()->isDBServer()) {
    _trx->state()->acceptAnalyzersRevision(analyzersRevision);
  }
  Result res = _trx->begin();
  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  enterState(QueryExecutionState::ValueType::PARSING);
  
  SerializationFormat format = SerializationFormat::SHADOWROWS;

  const bool planRegisters = !_queryString.empty();
  auto instantiateSnippet = [&](VPackSlice snippet) {
    auto plan = ExecutionPlan::instantiateFromVelocyPack(_ast.get(), snippet);
    TRI_ASSERT(plan != nullptr);

    plan->findVarUsage(); // I think this is a no-op

    ExecutionEngine::instantiateFromPlan(*this, *plan, planRegisters, format);
    _plans.push_back(std::move(plan));
  };

  answerBuilder.add("snippets", VPackValue(VPackValueType::Object));
  for (auto pair : VPackObjectIterator(snippets, /*sequential*/true)) {
    instantiateSnippet(pair.value);

    TRI_ASSERT(!_snippets.empty());
    TRI_ASSERT(!_trx->state()->isDBServer() ||
               _snippets.back()->engineId() != 0);

    answerBuilder.add(pair.key);
    answerBuilder.add(VPackValue(std::to_string(_snippets.back()->engineId())));
  }
  answerBuilder.close(); // snippets

  if (!_snippets.empty()) {
    TRI_ASSERT(_trx->state()->isDBServer() || _snippets[0]->engineId() == 0);
    // simon: just a hack for AQL_EXECUTEJSON
    if (_trx->state()->isCoordinator()) {  // register coordinator snippets
      TRI_ASSERT(_trx->state()->isCoordinator());
      QueryRegistryFeature::registry()->registerSnippets(_snippets);
    }
  }

  if (traverserSlice.isArray()) {
    // used to be RestAqlHandler::registerTraverserEngines
    answerBuilder.add("traverserEngines", VPackValue(VPackValueType::Array));
    for (auto const& te : VPackArrayIterator(traverserSlice)) {

      auto engine = traverser::BaseEngine::BuildEngine(_vocbase, *this, te);
      answerBuilder.add(VPackValue(engine->engineId()));

      _traversers.emplace_back(std::move(engine));
    }
    answerBuilder.close();  // traverserEngines
  }
  TRI_ASSERT(_trx != nullptr);
  
  if (_queryProfile) {  // simon: just a hack for AQL_EXECUTEJSON
    _queryProfile->registerInQueryList();
  }
  enterState(QueryExecutionState::ValueType::EXECUTION);
}

futures::Future<Result> ClusterQuery::finalizeClusterQuery(int errorCode) {
  TRI_ASSERT(_trx);
  TRI_ASSERT(ServerState::instance()->isDBServer());
  
  // technically there is no need for this in DBServers, but it should
  // be good practice to prevent the other cleanup code from running
  ShutdownState exp = ShutdownState::None;
  if (!_shutdownState.compare_exchange_strong(exp, ShutdownState::InProgress)) {
    return Result{TRI_ERROR_INTERNAL, "query already finalized"}; // someone else got here
  }

  LOG_TOPIC("fc33c", DEBUG, Logger::QUERIES)
       << elapsedSince(_startTime)
       << " Query::finalizeSnippets: before _trx->commit, errorCode: "
       << errorCode << ", this: " << (uintptr_t)this;
  
  enterState(QueryExecutionState::ValueType::FINALIZATION);
  
  for (auto& engine : _snippets) {  // make sure all snippets are unused
    engine->sharedState()->invalidate();
    engine->collectExecutionStats(_execStats);
  }

  // Use async API, commit on followers sends a request
  futures::Future<Result> finishResult(Result{});
  if (_trx->status() == transaction::Status::RUNNING) {
    if (errorCode == TRI_ERROR_NO_ERROR) {
      // no error. we need to commit the transaction
      finishResult = _trx->commitAsync();
    } else {
      // got an error. we need to abort the transaction
      finishResult = _trx->abortAsync();
    }
  }
  
  return std::move(finishResult).thenValue([this](Result res) -> Result {
    LOG_TOPIC("8ea28", DEBUG, Logger::QUERIES)
          << elapsedSince(_startTime)
          << " Query::finalizeSnippets post commit()"
          << " this: " << (uintptr_t)this;

     _execStats.requests += _numRequests.load(std::memory_order_relaxed);
     _execStats.setPeakMemoryUsage(_resourceMonitor.peakMemoryUsage());
     _execStats.setExecutionTime(elapsedSince(_startTime));
    _shutdownState.store(ShutdownState::Done);
     
     LOG_TOPIC("5fde0", DEBUG, Logger::QUERIES)
         << elapsedSince(_startTime)
         << " ClusterQuery::finalizeClusterQuery: done"
         << " this: " << (uintptr_t)this;
         
    return res;
  });
 }

