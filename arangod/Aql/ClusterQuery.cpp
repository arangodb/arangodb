////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
#include "Aql/QueryCache.h"
#include "Aql/QueryRegistry.h"
#include "Aql/QueryProfile.h"
#include "Aql/SharedQueryState.h"
#include "Basics/ScopeGuard.h"
#include "Cluster/ServerState.h"
#include "Cluster/TraverserEngine.h"
#include "Logger/LogMacros.h"
#include "Random/RandomGenerator.h"
#include "RestServer/QueryRegistryFeature.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Context.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Iterator.h>

using namespace arangodb;
using namespace arangodb::aql;

// Wait 2s to get the Lock in FastPath, otherwise assume dead-lock.
const double FAST_PATH_LOCK_TIMEOUT = 2.0;

ClusterQuery::ClusterQuery(QueryId id,
                           std::shared_ptr<transaction::Context> ctx,
                           QueryOptions options)
    : Query{id,
            ctx,
            {},
            /*bindParams*/ nullptr,
            std::move(options),
            /*sharedState*/ ServerState::instance()->isDBServer()
                ? nullptr
                : std::make_shared<SharedQueryState>(ctx->vocbase().server())},
      _planMemoryUsage(0) {}

ClusterQuery::~ClusterQuery() {
  _resourceMonitor.decreaseMemoryUsage(_planMemoryUsage);

  try {
    _traversers.clear();
  } catch (...) {
  }
}

/// @brief factory method for creating a cluster query. this must be used to
/// ensure that ClusterQuery objects are always created using shared_ptrs.
std::shared_ptr<ClusterQuery> ClusterQuery::create(
    QueryId id, std::shared_ptr<transaction::Context> ctx,
    QueryOptions options) {
  // workaround to enable make_shared on a class with a protected constructor
  struct MakeSharedQuery final : ClusterQuery {
    MakeSharedQuery(QueryId id, std::shared_ptr<transaction::Context> ctx,
                    QueryOptions options)
        : ClusterQuery{id, std::move(ctx), std::move(options)} {}

    ~MakeSharedQuery() final {
      // Destroy this query, otherwise it's still
      // accessible while the query is being destructed,
      // which can result in a data race on the vptr
      destroy();
    }
  };
  TRI_ASSERT(ctx != nullptr);
  return std::make_shared<MakeSharedQuery>(id, std::move(ctx),
                                           std::move(options));
}

void ClusterQuery::prepareClusterQuery(
    VPackSlice querySlice, VPackSlice collections, VPackSlice variables,
    VPackSlice snippets, VPackSlice traverserSlice, std::string const& user,
    VPackBuilder& answerBuilder,
    QueryAnalyzerRevisions const& analyzersRevision, bool fastPathLocking) {
  LOG_TOPIC("9636f", DEBUG, Logger::QUERIES)
      << elapsedSince(_startTime) << " ClusterQuery::prepareClusterQuery"
      << " this: " << (uintptr_t)this;

  // track memory usage
  ResourceUsageScope scope(_resourceMonitor);
  scope.increase(querySlice.byteSize() + collections.byteSize() +
                 variables.byteSize() + snippets.byteSize() +
                 traverserSlice.byteSize());

  _planMemoryUsage += scope.trackedAndSteal();

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

#ifdef USE_ENTERPRISE
  waitForSatellites();
#endif

  _trx = AqlTransaction::create(_transactionContext, _collections,
                                _queryOptions.transactionOptions,
                                std::move(inaccessibleCollections));

  // create the transaction object, but do not start the transaction yet
  _trx->addHint(
      transaction::Hints::Hint::FROM_TOPLEVEL_AQL);  // only used on toplevel
  if (_trx->state()->isDBServer()) {
    _trx->state()->acceptAnalyzersRevision(analyzersRevision);
    _trx->setUsername(user);
  }

  double origLockTimeout = _trx->state()->options().lockTimeout;
  if (fastPathLocking) {
    _trx->state()->options().lockTimeout = FAST_PATH_LOCK_TIMEOUT;
  }

  Result res = _trx->begin();
  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  _trx->state()->options().lockTimeout = origLockTimeout;

  TRI_IF_FAILURE("Query::setupLockTimeout") {
    if (!_trx->state()->isReadOnlyTransaction() &&
        RandomGenerator::interval(uint32_t(100)) >= 95) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_LOCK_TIMEOUT);
    }
  }

  if (ServerState::instance()->isDBServer()) {
    _collections.visit([&](std::string const&,
                           aql::Collection const& c) -> bool {
      // this code will only execute on leaders
      _trx->state()->trackShardRequest(*_trx->resolver(), _vocbase.name(),
                                       c.name(), user, c.accessType(), "aql");
      return true;
    });
  }

  enterState(QueryExecutionState::ValueType::PARSING);

  bool const planRegisters = !_queryString.empty();
  auto instantiateSnippet = [&](VPackSlice snippet) {
    auto plan = ExecutionPlan::instantiateFromVelocyPack(_ast.get(), snippet);
    TRI_ASSERT(plan != nullptr);

    plan->findVarUsage();  // I think this is a no-op

    ExecutionEngine::instantiateFromPlan(*this, *plan, planRegisters);
    _plans.push_back(std::move(plan));
  };

  answerBuilder.add("snippets", VPackValue(VPackValueType::Object));
  for (auto pair : VPackObjectIterator(snippets, /*sequential*/ true)) {
    instantiateSnippet(pair.value);

    TRI_ASSERT(!_snippets.empty());
    TRI_ASSERT(!_trx->state()->isDBServer() ||
               _snippets.back()->engineId() != 0);

    answerBuilder.add(pair.key);
    answerBuilder.add(VPackValue(std::to_string(_snippets.back()->engineId())));
  }
  answerBuilder.close();  // snippets

  if (!_snippets.empty()) {
    TRI_ASSERT(_trx->state()->isDBServer() || _snippets[0]->engineId() == 0);
    // simon: just a hack for AQL_EXECUTEJSON
    if (_trx->state()->isCoordinator()) {  // register coordinator snippets
      TRI_ASSERT(_trx->state()->isCoordinator());
      QueryRegistryFeature::registry()->registerSnippets(_snippets);
    }

    registerQueryInTransactionState();
  }

  if (traverserSlice.isArray()) {
    // used to be RestAqlHandler::registerTraverserEngines
    answerBuilder.add("traverserEngines", VPackValue(VPackValueType::Array));
    for (auto const te : VPackArrayIterator(traverserSlice)) {
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

futures::Future<Result> ClusterQuery::finalizeClusterQuery(
    ErrorCode errorCode) {
  TRI_ASSERT(_trx);
  TRI_ASSERT(ServerState::instance()->isDBServer());

  // technically there is no need for this in DBServers, but it should
  // be good practice to prevent the other cleanup code from running
  ShutdownState exp = ShutdownState::None;
  if (!_shutdownState.compare_exchange_strong(exp, ShutdownState::InProgress)) {
    return Result{TRI_ERROR_INTERNAL,
                  "query already finalized"};  // someone else got here
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
        << elapsedSince(_startTime) << " Query::finalizeSnippets post commit()"
        << " this: " << (uintptr_t)this;

    _execStats.requests += _numRequests.load(std::memory_order_relaxed);
    _execStats.setPeakMemoryUsage(_resourceMonitor.peak());
    _execStats.setExecutionTime(elapsedSince(_startTime));
    _execStats.setIntermediateCommits(_trx->state()->numIntermediateCommits());
    _shutdownState.store(ShutdownState::Done);

    unregisterQueryInTransactionState();

    LOG_TOPIC("5fde0", DEBUG, Logger::QUERIES)
        << elapsedSince(_startTime)
        << " ClusterQuery::finalizeClusterQuery: done"
        << " this: " << (uintptr_t)this;

    return res;
  });
}
