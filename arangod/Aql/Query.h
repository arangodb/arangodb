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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_QUERY_H
#define ARANGOD_AQL_QUERY_H 1

#include "Aql/AqlItemBlockManager.h"
#include "Aql/BindParameters.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/ExecutionState.h"
#include "Aql/QueryContext.h"
#include "Aql/QueryExecutionState.h"
#include "Aql/QueryResultV8.h"
#include "Aql/QueryString.h"
#include "Aql/ResourceUsage.h"
#include "Aql/SharedQueryState.h"
#include "Basics/Common.h"
#include "V8Server/V8Context.h"

#include <velocypack/Builder.h>

struct TRI_vocbase_t;

namespace arangodb {
class CollectionNameResolver;
class LogicalDataSource;  // forward declaration

namespace transaction {
class Context;
class Methods;
}  // namespace transaction

namespace velocypack {
class Builder;
}

namespace graph {
class Graph;
}

namespace aql {

struct AstNode;
class Ast;
class ExecutionEngine;
class Query;
struct QueryCacheResultEntry;
struct QueryProfile;
enum class SerializationFormat;

/// @brief an AQL query
class Query : public QueryContext {
 private:
  enum ExecutionPhase { INITIALIZE, EXECUTE, FINALIZE };

  Query(Query const&) = delete;
  Query& operator=(Query const&) = delete;

 public:
  /// Used to construct a full query
  Query(std::shared_ptr<transaction::Context> const& ctx, QueryString const& queryString,
        std::shared_ptr<arangodb::velocypack::Builder> const& bindParameters,
        std::shared_ptr<arangodb::velocypack::Builder> const& options);

  /// Used to put together query snippets in RestAqlHandler
//  Query(bool contextOwnedByExterior, TRI_vocbase_t& vocbase,
//        std::shared_ptr<arangodb::velocypack::Builder> const& queryStruct,
//        std::shared_ptr<arangodb::velocypack::Builder> const& options, QueryPart);

  virtual ~Query();

  /// @brief note that the query uses the DataSource
  void addDataSource(std::shared_ptr<arangodb::LogicalDataSource> const& ds);

  /// @brief clone a query
  /// note: as a side-effect, this will also create and start a transaction for
  /// the query
//  TEST_VIRTUAL Query* clone(QueryPart, bool withPlan);

  constexpr static uint64_t DontCache = 0;

  /// @brief whether or not the query is killed
  bool killed() const override;
  
  void setKilled() override {
    _killed = true;
  }

  /// @brief set the query to killed
  void kill();

  void setExecutionTime();

  QueryString const& queryString() const { return _queryString; }

  QueryProfile* profile() const { return _profile.get(); }

  velocypack::Slice optionsSlice() const { return _options->slice(); }
  
  TEST_VIRTUAL QueryOptions& queryOptions() { return _queryOptions; }

  /// @brief return the start timestamp of the query
  double startTime() const { return _startTime; }

  void prepare(SerializationFormat format);

  /// @brief execute an AQL query
  aql::ExecutionState execute(QueryResult& res);

  /// @brief execute an AQL query and block this thread in case we
  ///        need to wait.
  QueryResult executeSync();

  /// @brief execute an AQL query
  /// may only be called with an active V8 handle scope
  QueryResultV8 executeV8(v8::Isolate* isolate);

  /// @brief Enter finalization phase and do cleanup.
  /// Sets `warnings`, `stats`, `profile`, timings and does the cleanup.
  /// Only use directly for a streaming query, rather use `execute(...)`
  ExecutionState finalize(QueryResult&);

  /// @brief parse an AQL query
  QueryResult parse();

  /// @brief explain an AQL query
  QueryResult explain();

  /// @brief return the engine, if prepared
//  TEST_VIRTUAL ExecutionEngine* engine() const { return _engine.get(); }

  /// @brief inject the engine
//  TEST_VIRTUAL void setEngine(ExecutionEngine* engine);
//  TEST_VIRTUAL void setEngine(std::unique_ptr<ExecutionEngine>&& engine);

  /// @brief get the plan for the query
//  ExecutionPlan* plan() const { return _plan.get(); }

  /// @brief whether or not a query is a modification query
  bool isModificationQuery() const;

  /// @brief mark a query as modification query
  void setIsModificationQuery();
  
  void setIsAsyncQuery() { _isAsyncQuery = true; }

  /// @brief prepare a V8 context for execution for this expression
  /// this needs to be called once before executing any V8 function in this
  /// expression
  virtual void prepareV8Context() override;

  /// @brief enter a V8 context
  virtual void enterV8Context() override;

  /// @brief exits a V8 context
  virtual void exitV8Context() override;

  // @brief resets the contexts load-state of the AQL functions.
  virtual void unPrepareV8Context() override { _preparedV8Context = false; }
  
  /// @brief check if the query has a V8 context ready for use
  virtual bool hasEnteredV8Context() const override {
    return (_contextOwnedByExterior || _V8Context != nullptr);
  }

  /// @brief returns statistics for current query.
  void getStats(arangodb::velocypack::Builder&);

  /// @brief return the bind parameters as passed by the user
  std::shared_ptr<arangodb::velocypack::Builder> bindParameters() const {
    return _bindParameters.builder();
  }

  /// @brief return the query's shared state
  std::shared_ptr<SharedQueryState> sharedState() const;
  ExecutionEngine* engine() const;
  
public:
  
  virtual QueryOptions const& queryOptions() const override {
    return _queryOptions;
  }
  
  /// @brief pass-thru a resolver object from the transaction context
  virtual CollectionNameResolver const& resolver() const override;
  
  /// @brief create a transaction::Context
  virtual std::shared_ptr<transaction::Context> newTrxContext() const override;
  
  virtual velocypack::Options const& vpackOptions() const override;
  
  virtual transaction::Methods& trxForOptimization() override;
  
  virtual Result commitOperations() override;

 private:
  /// @brief initializes the query
  void init();

  /// @brief calculate a hash for the query, once
  uint64_t hash() const;

  /// @brief prepare an AQL query, this is a preparation for execute, but
  /// execute calls it internally. The purpose of this separate method is
  /// to be able to only prepare a query from VelocyPack and then store it in
  /// the QueryRegistry.
  std::unique_ptr<ExecutionPlan> preparePlan();

  /// @brief log a query
  void log();

  /// @brief calculate a hash value for the query string and bind parameters
  uint64_t calculateHash() const;

  /// @brief whether or not the query cache can be used for the query
  bool canUseQueryCache() const;

  /// @brief enter a new state
  void enterState(QueryExecutionState::ValueType);

  /// @brief cleanup plan and engine for current query. synchronous variant,
  /// will block this thread in WAITING case.
  void cleanupPlanAndEngineSync(int errorCode, velocypack::Builder* statsBuilder = nullptr) noexcept;

  /// @brief cleanup plan and engine for current query can issue WAITING
  ExecutionState cleanupPlanAndEngine(int errorCode,
                                      velocypack::Builder* statsBuilder = nullptr);

 private:
  
  AqlItemBlockManager _itemBlockMananger;
  
  /// @brief the actual query string
  QueryString _queryString;

  /// @brief transaction context to use for this query
  std::shared_ptr<transaction::Context> _transactionContext;

  /// @brief the currently used V8 context
  V8Context* _V8Context;

  /// @brief query in a VelocyPack structure
  std::shared_ptr<arangodb::velocypack::Builder> const _queryBuilder;

  /// @brief bind parameters for the query
  BindParameters _bindParameters;

  /// @brief raw query options
  std::shared_ptr<arangodb::velocypack::Builder> _options;
  
  /// @brief parsed query options
  QueryOptions _queryOptions;
  
  /// @brief first one should be the local one
  aql::SnippetList _snippets;
  
  /// @brief query execution profile
  std::unique_ptr<QueryProfile> _profile;

  /// @brief _ast, we need an ast to manage the memory for AstNodes, even
  /// if we do not have a parser, because AstNodes occur in plans and engines
  std::unique_ptr<Ast> _ast;
  
  /// @brief the ExecutionPlan object, if the query is prepared
  std::vector<std::unique_ptr<ExecutionPlan>> _plans;

  /// @brief the transaction object, in a distributed query every part of
  /// the query has its own transaction object. The transaction object is
  /// created in the prepare method.
  std::unique_ptr<transaction::Methods> _trx;
  
  /// Create the result in this builder. It is also used to determine
  /// if we are continuing the query or of we called
  std::shared_ptr<arangodb::velocypack::Builder> _resultBuilder;

  /// Options for _resultBuilder. Optimally, its lifetime should be linked to
  /// it, but this is hard to do.
  std::unique_ptr<arangodb::velocypack::Options> _resultBuilderOptions;
  
  /// @brief query cache entry built by the query
  /// only populated when the query has generated its result(s) and before
  /// storing the cache entry in the query cache
  std::unique_ptr<QueryCacheResultEntry> _cacheEntry;

  /// @brief query start time
  double _startTime;

  /// @brief hash for this query. will be calculated only once when needed
  mutable uint64_t _queryHash = DontCache;
  
  /// Track in which phase of execution we are, in order to implement
  /// repeatability.
  ExecutionPhase _executionPhase;
  
  /// @brief whether or not someone else has acquired a V8 context for us
  bool const _contextOwnedByExterior;
  
  bool _killed;
  
  /// @brief does this query contain async execution nodes
  bool _isAsyncQuery;

  /// @brief whether or not the preparation routine for V8 contexts was run
  /// once for this expression
  /// it needs to be run once before any V8-based function is called
  bool _preparedV8Context;

  /// @brief whether or not the hash was already calculated
  mutable bool _queryHashCalculated;
};

}  // namespace aql
}  // namespace arangodb

#endif
