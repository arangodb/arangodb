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

#ifndef ARANGOD_AQL_QUERY_H
#define ARANGOD_AQL_QUERY_H 1

#include "Aql/BindParameters.h"
#include "Aql/Collections.h"
#include "Aql/ExecutionState.h"
#include "Aql/Graphs.h"
#include "Aql/QueryExecutionState.h"
#include "Aql/QueryOptions.h"
#include "Aql/QueryResources.h"
#include "Aql/QueryResultV8.h"
#include "Aql/QueryString.h"
#include "Aql/RegexCache.h"
#include "Aql/ResourceUsage.h"
#include "Aql/SharedQueryState.h"
#include "Aql/types.h"
#include "Basics/Common.h"
#include "Basics/ConditionLocker.h"
#include "Basics/ConditionVariable.h"
#include "V8Server/V8Context.h"
#include "VocBase/voc-types.h"

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
class ExecutionPlan;
class Query;
struct QueryCacheResultEntry;
struct QueryProfile;
class QueryRegistry;

/// @brief query part
enum QueryPart { PART_MAIN, PART_DEPENDENT };

/// @brief an AQL query
class Query {
 private:
  enum ExecutionPhase { INITIALIZE, EXECUTE, FINALIZE };

  Query(Query const&) = delete;
  Query& operator=(Query const&) = delete;

 public:
  /// Used to construct a full query
  Query(bool contextOwnedByExterior, TRI_vocbase_t& vocbase, QueryString const& queryString,
        std::shared_ptr<arangodb::velocypack::Builder> const& bindParameters,
        std::shared_ptr<arangodb::velocypack::Builder> const& options, QueryPart part);

  /// Used to put together query snippets in RestAqlHandler
  Query(bool contextOwnedByExterior, TRI_vocbase_t& vocbase,
        std::shared_ptr<arangodb::velocypack::Builder> const& queryStruct,
        std::shared_ptr<arangodb::velocypack::Builder> const& options, QueryPart);

  virtual ~Query();

  /// @brief note that the query uses the DataSource
  void addDataSource(std::shared_ptr<arangodb::LogicalDataSource> const& ds);

  /// @brief clone a query
  /// note: as a side-effect, this will also create and start a transaction for
  /// the query
  TEST_VIRTUAL Query* clone(QueryPart, bool);

  constexpr static uint64_t DontCache = 0;

  /// @brief whether or not the query is killed
  inline bool killed() const { return _killed; }

  /// @brief set the query to killed
  void kill();

  /// @brief increase number of HTTP requests. this is normally
  /// called during the setup of a query
  void incHttpRequests(size_t requests);

  void setExecutionTime();

  QueryString const& queryString() const { return _queryString; }

  /// @brief Inject a transaction from outside. Use with care!
  void injectTransaction(std::shared_ptr<transaction::Methods> trx) {
    _trx = std::move(trx);
    init();
  }

  /// @brief inject a transaction context to use
  void setTransactionContext(std::shared_ptr<transaction::Context> const& ctx) {
    _transactionContext = ctx;
  }

  QueryProfile* profile() const { return _profile.get(); }

  velocypack::Slice optionsSlice() const { return _options->slice(); }
  TEST_VIRTUAL QueryOptions const& queryOptions() const {
    return _queryOptions;
  }
  TEST_VIRTUAL QueryOptions& queryOptions() { return _queryOptions; }

  void increaseMemoryUsage(size_t value) {
    _resourceMonitor.increaseMemoryUsage(value);
  }
  void decreaseMemoryUsage(size_t value) {
    _resourceMonitor.decreaseMemoryUsage(value);
  }

  ResourceMonitor* resourceMonitor() { return &_resourceMonitor; }

  /// @brief return the start timestamp of the query
  double startTime() const { return _startTime; }

  /// @brief the part of the query
  inline QueryPart part() const { return _part; }

  /// @brief get the vocbase
  inline TRI_vocbase_t& vocbase() const { return _vocbase; }

  inline Collection* addCollection(std::string const& name, AccessMode::Type accessType) {
    // Either collection or view
    return _collections.add(name, accessType);
  }

  inline Collection* addCollection(arangodb::velocypack::StringRef name, AccessMode::Type accessType) {
    return _collections.add(name.toString(), accessType);
  }

  /// @brief collections
  inline Collections const* collections() const { return &_collections; }

  /// @brief return the names of collections used in the query
  std::vector<std::string> collectionNames() const {
    return _collections.collectionNames();
  }

  /// @brief return the query's id
  TRI_voc_tick_t id() const { return _id; }

  /// @brief getter for _ast
  Ast* ast() const { return _ast.get(); }

  /// @brief add a node to the list of nodes
  void addNode(AstNode* node) { _resources.addNode(node); }

  /// @brief register a string
  /// the string is freed when the query is destroyed
  char* registerString(char const* p, size_t length) {
    return _resources.registerString(p, length);
  }

  /// @brief register a string
  /// the string is freed when the query is destroyed
  char* registerString(std::string const& value) {
    return _resources.registerString(value);
  }

  /// @brief register a potentially UTF-8-escaped string
  /// the string is freed when the query is destroyed
  char* registerEscapedString(char const* p, size_t length, size_t& outLength) {
    return _resources.registerEscapedString(p, length, outLength);
  }

  /// @brief register an error, with an optional parameter inserted into printf
  /// this also makes the query abort
  void registerError(int, char const* = nullptr);

  /// @brief register an error with a custom error message
  /// this also makes the query abort
  void registerErrorCustom(int, char const*);

  /// @brief register a warning
  TEST_VIRTUAL void registerWarning(int, char const* = nullptr);

  /// @brief register a warning (convenience overload)
  TEST_VIRTUAL void registerWarning(int code, std::string const& details);

  void prepare(QueryRegistry*);

  /// @brief execute an AQL query
  aql::ExecutionState execute(QueryRegistry*, QueryResult& res);

  /// @brief execute an AQL query and block this thread in case we
  ///        need to wait.
  QueryResult executeSync(QueryRegistry*);

  /// @brief execute an AQL query
  /// may only be called with an active V8 handle scope
  aql::ExecutionState executeV8(v8::Isolate* isolate, QueryRegistry*, QueryResultV8&);

  /// @brief Enter finalization phase and do cleanup.
  /// Sets `warnings`, `stats`, `profile`, timings and does the cleanup.
  /// Only use directly for a streaming query, rather use `execute(...)`
  ExecutionState finalize(QueryResult&);

  /// @brief parse an AQL query
  QueryResult parse();

  /// @brief explain an AQL query
  QueryResult explain();

  /// @brief cache for regular expressions constructed by the query
  RegexCache* regexCache() { return &_regexCache; }

  /// @brief return the engine, if prepared
  TEST_VIRTUAL ExecutionEngine* engine() const { return _engine.get(); }

  /// @brief inject the engine
  TEST_VIRTUAL void setEngine(ExecutionEngine* engine);

  /// @brief return the transaction, if prepared
  TEST_VIRTUAL inline transaction::Methods* trx() { return _trx.get(); }

  /// @brief get the plan for the query
  ExecutionPlan* plan() const { return _plan.get(); }

  /// @brief whether or not a query is a modification query
  bool isModificationQuery() const { return _isModificationQuery; }

  /// @brief mark a query as modification query
  void setIsModificationQuery() { _isModificationQuery = true; }

  /// @brief prepare a V8 context for execution for this expression
  /// this needs to be called once before executing any V8 function in this
  /// expression
  void prepareV8Context();

  /// @brief enter a V8 context
  void enterContext();

  /// @brief exits a V8 context
  void exitContext();

  /// @brief check if the query has a V8 context ready for use
  bool hasEnteredContext() const {
    return (_contextOwnedByExterior || _context != nullptr);
  }

  // @brief resets the contexts load-state of the AQL functions.
  void unPrepareV8Context() { _preparedV8Context = false; }

  /// @brief returns statistics for current query.
  void getStats(arangodb::velocypack::Builder&);

  /// @brief add the list of warnings to VelocyPack.
  ///        Will add a new entry { ..., warnings: <warnings>, } if there are
  ///        warnings. If there are none it will not modify the builder
  void addWarningsToVelocyPack(arangodb::velocypack::Builder&) const;

  /// @brief look up a graph in the _graphs collection
  graph::Graph const* lookupGraphByName(std::string const& name);

  /// @brief return the bind parameters as passed by the user
  std::shared_ptr<arangodb::velocypack::Builder> bindParameters() const {
    return _bindParameters.builder();
  }

  QueryExecutionState::ValueType state() const { return _state; }

  /// @brief return the query's shared state
  std::shared_ptr<SharedQueryState> sharedState() const { return _sharedState; }

  /// @brief pass-thru a resolver object from the transaction context
  CollectionNameResolver const& resolver();

 private:
  /// @brief initializes the query
  void init();

  /// @brief calculate a hash for the query, once
  uint64_t hash() const;

  /// @brief prepare an AQL query, this is a preparation for execute, but
  /// execute calls it internally. The purpose of this separate method is
  /// to be able to only prepare a query from VelocyPack and then store it in
  /// the QueryRegistry.
  ExecutionPlan* preparePlan();

  /// @brief log a query
  void log();

  /// @brief calculate a hash value for the query string and bind parameters
  uint64_t calculateHash() const;

  /// @brief whether or not the query cache can be used for the query
  bool canUseQueryCache() const;

  /// @brief neatly format exception messages for the users
  std::string buildErrorMessage(int errorCode) const;

  /// @brief enter a new state
  void enterState(QueryExecutionState::ValueType);

  /// @brief cleanup plan and engine for current query. synchronous variant,
  /// will block this thread in WAITING case.
  void cleanupPlanAndEngineSync(int errorCode, VPackBuilder* statsBuilder = nullptr) noexcept;

  /// @brief cleanup plan and engine for current query can issue WAITING
  ExecutionState cleanupPlanAndEngine(int errorCode, VPackBuilder* statsBuilder = nullptr);

  /// @brief create a transaction::Context
  std::shared_ptr<transaction::Context> createTransactionContext();

  /// @brief returns the next query id
  static TRI_voc_tick_t nextId();

 private:
  /// @brief query id
  TRI_voc_tick_t _id;

  /// @brief current resources and limits used by query
  ResourceMonitor _resourceMonitor;

  /// @brief resources used by query
  QueryResources _resources;

  /// @brief pointer to vocbase the query runs in
  TRI_vocbase_t& _vocbase;

  /// @brief transaction context to use for this query
  std::shared_ptr<transaction::Context> _transactionContext;

  /// @brief the currently used V8 context
  V8Context* _context;

  /// @brief graphs used in query, identified by name
  std::unordered_map<std::string, std::unique_ptr<graph::Graph>> _graphs;

  /// @brief set of DataSources used in the query
  ///        needed for the query cache, stores datasource guid -> datasource name
  std::unordered_map<std::string, std::string> _queryDataSources;

  /// @brief the actual query string
  QueryString _queryString;

  /// @brief query in a VelocyPack structure
  std::shared_ptr<arangodb::velocypack::Builder> const _queryBuilder;

  /// @brief bind parameters for the query
  BindParameters _bindParameters;

  /// @brief raw query options
  std::shared_ptr<arangodb::velocypack::Builder> _options;

  /// @brief parsed query options
  QueryOptions _queryOptions;

  /// @brief collections used in the query
  Collections _collections;

  /// @brief _ast, we need an ast to manage the memory for AstNodes, even
  /// if we do not have a parser, because AstNodes occur in plans and engines
  std::unique_ptr<Ast> _ast;

  /// @brief query execution profile
  std::unique_ptr<QueryProfile> _profile;

  /// @brief current state the query is in (used for profiling and error
  /// messages)
  QueryExecutionState::ValueType _state;

  /// @brief the ExecutionPlan object, if the query is prepared
  std::shared_ptr<ExecutionPlan> _plan;

  /// @brief the transaction object, in a distributed query every part of
  /// the query has its own transaction object. The transaction object is
  /// created in the prepare method.
  std::shared_ptr<transaction::Methods> _trx;
  bool _isClonedQuery = false;

  /// @brief the ExecutionEngine object, if the query is prepared
  std::unique_ptr<ExecutionEngine> _engine;

  /// @brief warnings collected during execution
  std::vector<std::pair<int, std::string>> _warnings;

  /// @brief cache for regular expressions constructed by the query
  RegexCache _regexCache;

  /// @brief query start time
  double _startTime;

  /// @brief the query part
  QueryPart const _part;

  /// @brief whether or not someone else has acquired a V8 context for us
  bool const _contextOwnedByExterior;

  /// @brief whether or not the query is killed
  bool _killed;

  /// @brief whether or not the query is a data modification query
  bool _isModificationQuery;

  /// @brief whether or not the preparation routine for V8 contexts was run
  /// once for this expression
  /// it needs to be run once before any V8-based function is called
  bool _preparedV8Context;

  /// Create the result in this builder. It is also used to determine
  /// if we are continuing the query or of we called
  std::shared_ptr<arangodb::velocypack::Builder> _resultBuilder;

  /// Options for _resultBuilder. Optimally, its lifetime should be linked to
  /// it, but this is hard to do.
  std::shared_ptr<arangodb::velocypack::Options> _resultBuilderOptions;

  /// Track in which phase of execution we are, in order to implement
  /// repeatability.
  ExecutionPhase _executionPhase;

  /// @brief shared state
  std::shared_ptr<SharedQueryState> _sharedState;

  /// @brief query cache entry built by the query
  /// only populated when the query has generated its result(s) and before
  /// storing the cache entry in the query cache
  std::unique_ptr<QueryCacheResultEntry> _cacheEntry;

  /// @brief hash for this query. will be calculated only once when needed
  mutable uint64_t _queryHash = DontCache;

  /// @brief whether or not the hash was already calculated
  mutable bool _queryHashCalculated = false;
};

}  // namespace aql
}  // namespace arangodb

#endif
