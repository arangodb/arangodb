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
#include "Aql/SharedQueryState.h"
#include "Basics/Common.h"
#include "Basics/system-functions.h"
#include "V8Server/V8Context.h"
#include "Cluster/ClusterTypes.h"

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

class Ast;
struct AstNode;
class ExecutionEngine;
struct ExecutionStats;
struct QueryCacheResultEntry;
struct QueryProfile;
enum class SerializationFormat;

/// @brief an AQL query
class Query : public QueryContext {
 private:
  enum ExecutionPhase { INITIALIZE, EXECUTE, FINALIZE };

  Query(Query const&) = delete;
  Query& operator=(Query const&) = delete;

 protected:
  /// @brief internal constructor, Used to construct a full query or a ClusterQuery
  Query(std::shared_ptr<transaction::Context> const& ctx, QueryString const& queryString,
        std::shared_ptr<arangodb::velocypack::Builder> const& bindParameters,
        std::shared_ptr<arangodb::velocypack::Builder> const& options,
        std::shared_ptr<SharedQueryState> sharedState);

 public:
  /// @brief public constructor, Used to construct a full query
  Query(std::shared_ptr<transaction::Context> const& ctx, QueryString const& queryString,
        std::shared_ptr<arangodb::velocypack::Builder> const& bindParameters,
        std::shared_ptr<arangodb::velocypack::Builder> const& options);

  virtual ~Query();

  /// @brief note that the query uses the DataSource
  void addDataSource(std::shared_ptr<arangodb::LogicalDataSource> const& ds);

  constexpr static uint64_t DontCache = 0;

  /// @brief return the user that started the query
  std::string const& user() const override;

  /// @brief whether or not the query is killed
  bool killed() const override;

  /// @brief set the query to killed
  void kill();

  /// @brief setter and getter methods for the query lockTimeout. 
  void setLockTimeout(double timeout) noexcept override;
  double getLockTimeout() const noexcept override;

  QueryString const& queryString() const { return _queryString; }
  
  QueryProfile* profile() const { return _profile.get(); }

  velocypack::Slice optionsSlice() const { return _options->slice(); }
  
  TEST_VIRTUAL QueryOptions& queryOptions() { return _queryOptions; }

  /// @brief return the start time of the query (steady clock value)
  double startTime() const noexcept;

  void prepareQuery(SerializationFormat format);
  
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

  /// @brief whether or not a query is a modification query
  virtual bool isModificationQuery() const noexcept override;

  virtual bool isAsyncQuery() const noexcept override;

  /// @brief enter a V8 context
  virtual void enterV8Context() override;

  /// @brief exits a V8 context
  virtual void exitV8Context() override;

  /// @brief check if the query has a V8 context ready for use
  virtual bool hasEnteredV8Context() const override {
    return (_contextOwnedByExterior || _v8Context != nullptr);
  }

  /// @brief return the bind parameters as passed by the user
  std::shared_ptr<arangodb::velocypack::Builder> bindParameters() const {
    return _bindParameters.builder();
  }

  /// @brief return the query's shared state
  std::shared_ptr<SharedQueryState> sharedState() const;
  ExecutionEngine* rootEngine() const;
  
  Ast* ast() {
    return _ast.get();
  }
  
  virtual QueryOptions const& queryOptions() const override {
    return _queryOptions;
  }
  
  /// @brief pass-thru a resolver object from the transaction context
  virtual CollectionNameResolver const& resolver() const override;
  
  /// @brief create a transaction::Context
  virtual std::shared_ptr<transaction::Context> newTrxContext() const override;
  
  virtual velocypack::Options const& vpackOptions() const override;
  
  virtual transaction::Methods& trxForOptimization() override;
  
#ifdef ARANGODB_USE_GOOGLE_TESTS
  ExecutionPlan* plan() const {
    if (_plans.size() == 1) {
      return _plans[0].get();
    }
    return nullptr;
  }
  void initForTests();
#endif
  
  AqlItemBlockManager& itemBlockManager() {
    return _itemBlockManager;
  }
  
  SnippetList const& snippets() const {
    return _snippets;
  }

 protected:
  /// @brief initializes the query
  void init(bool createProfile);

  /// @brief calculate a hash for the query, once
  uint64_t hash();

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

  /// @brief cleanup plan and engine for current query can issue WAITING
  ExecutionState cleanupPlanAndEngine(int errorCode, bool sync,
                                      velocypack::Builder* statsBuilder = nullptr,
                                      bool includePlan = false);
  
  void unregisterSnippets();

 protected:
  
  AqlItemBlockManager _itemBlockManager;
  
  /// @brief the actual query string
  QueryString _queryString;

  /// @brief transaction context to use for this query
  std::shared_ptr<transaction::Context> _transactionContext;
  
  /// @brief shared query state
  std::shared_ptr<SharedQueryState> _sharedState;

  /// @brief the currently used V8 context
  V8Context* _v8Context;

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

  /// @brief query start time (steady clock value)
  double const _startTime;
  
  /// @brief total memory used for building the (partial) result
  size_t _resultMemoryUsage;

  /// @brief hash for this query. will be calculated only once when needed
  mutable uint64_t _queryHash = DontCache;

  /// @brief user that started the query
  std::string _user;
  
  /// Track in which phase of execution we are, in order to implement
  /// repeatability.
  ExecutionPhase _executionPhase;
  
  /// @brief whether or not someone else has acquired a V8 context for us
  bool const _contextOwnedByExterior;
  
  /// avoid killing a query in normal shutdown / cleanup
  enum class KillState : uint8_t {
    None, Shutdown, Killed
  };
  
  std::atomic<KillState> _killState;
  
  /// @brief whether or not the hash was already calculated
  bool _queryHashCalculated;
};

// additonally allows TraversalEngines
class ClusterQuery final : public Query {
 public:
  
  /// Used to construct a full query
  ClusterQuery(std::shared_ptr<transaction::Context> const& ctx,
               std::shared_ptr<arangodb::velocypack::Builder> const& options);
  ~ClusterQuery();
  
  traverser::GraphEngineList const& traversers() const {
    return _traversers;
  }
  
  void prepareClusterQuery(SerializationFormat format,
                           arangodb::velocypack::Slice querySlice,
                           arangodb::velocypack::Slice collections,
                           arangodb::velocypack::Slice variables,
                           arangodb::velocypack::Slice snippets,
                           arangodb::velocypack::Slice traversals,
                           arangodb::velocypack::Builder& answer,
                           arangodb::QueryAnalyzerRevisions const& analyzersRevision);
  
  Result finalizeClusterQuery(ExecutionStats& stats, int errorCode);

 private:
  /// @brief first one should be the local one
  traverser::GraphEngineList _traversers;
};

}  // namespace aql
}  // namespace arangodb

#endif
