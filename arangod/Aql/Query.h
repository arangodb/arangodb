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

#include "Basics/Common.h"

#include <velocypack/Builder.h>

#include "Aql/BindParameters.h"
#include "Aql/Collections.h"
#include "Aql/Graphs.h"
#include "Aql/QueryResources.h"
#include "Aql/QueryResultV8.h"
#include "Aql/ResourceUsage.h"
#include "Aql/types.h"
#include "Basics/Common.h"
#include "V8Server/V8Context.h"
#include "VocBase/voc-types.h"

struct TRI_vocbase_t;

namespace arangodb {
class Transaction;
class TransactionContext;

namespace velocypack {
class Builder;
}

namespace aql {

struct AstNode;
class Ast;
class ExecutionEngine;
class ExecutionPlan;
class Executor;
class Parser;
class Query;
class QueryRegistry;

/// @brief equery part
enum QueryPart { PART_MAIN, PART_DEPENDENT };

/// @brief execution states
enum ExecutionState {
  INITIALIZATION = 0,
  PARSING,
  AST_OPTIMIZATION,
  LOADING_COLLECTIONS,
  PLAN_INSTANTIATION,
  PLAN_OPTIMIZATION,
  EXECUTION,
  FINALIZATION,

  INVALID_STATE
};

struct Profile {
  Profile(Profile const&) = delete;
  Profile& operator=(Profile const&) = delete;

  explicit Profile(Query*);

  ~Profile();

  void setDone(ExecutionState);

  /// @brief convert the profile to VelocyPack
  std::shared_ptr<arangodb::velocypack::Builder> toVelocyPack();

  Query* query;
  std::vector<std::pair<ExecutionState, double>> results;
  double stamp;
  bool tracked;
};

/// @brief an AQL query
class Query {
 private:
  Query(Query const&) = delete;
  Query& operator=(Query const&) = delete;

 public:
  Query(bool, TRI_vocbase_t*, char const*, size_t,
        std::shared_ptr<arangodb::velocypack::Builder>,
        std::shared_ptr<arangodb::velocypack::Builder>, QueryPart);

  Query(bool, TRI_vocbase_t*,
        std::shared_ptr<arangodb::velocypack::Builder> const,
        std::shared_ptr<arangodb::velocypack::Builder>, QueryPart);

  ~Query();

  /// @brief clone a query
  /// note: as a side-effect, this will also create and start a transaction for
  /// the query
  Query* clone(QueryPart, bool);

 public:

  /// @brief Inject a transaction from outside. Use with care!
  void injectTransaction (arangodb::Transaction* trx) {
    _trx = trx;
    init();
  }

  void increaseMemoryUsage(size_t value) { _resourceMonitor.increaseMemoryUsage(value); }
  void decreaseMemoryUsage(size_t value) { _resourceMonitor.decreaseMemoryUsage(value); }
  
  ResourceMonitor* resourceMonitor() { return &_resourceMonitor; }

  /// @brief return the start timestamp of the query
  double startTime () const { return _startTime; }

  /// @brief whether or not the query is killed
  inline bool killed() const { return _killed; }

  /// @brief set the query to killed
  inline void killed(bool) { _killed = true; }

  /// @brief the part of the query
  inline QueryPart part() const { return _part; }

  /// @brief get the vocbase
  inline TRI_vocbase_t* vocbase() const { return _vocbase; }

  /// @brief collections
  inline Collections* collections() { return &_collections; }

  /// @brief return the names of collections used in the query
  std::vector<std::string> collectionNames() const {
    return _collections.collectionNames();
  }

  /// @brief return the query's id
  TRI_voc_tick_t id() const { return _id; }

  /// @brief get the query string
  char const* queryString() const { return _queryString; }

  /// @brief get the length of the query string
  size_t queryLength() const { return _queryLength; }

  /// @brief getter for _ast
  Ast* ast() const { return _ast; }

  /// @brief should we return verbose plans?
  bool verbosePlans() const { return getBooleanOption("verbosePlans", false); }

  /// @brief should we return all plans?
  bool allPlans() const { return getBooleanOption("allPlans", false); }

  /// @brief should the execution be profiled?
  bool profiling() const { return getBooleanOption("profile", false); }
  
  /// @brief should we suppress the query result (useful for performance testing only)?
  bool silent() const { return getBooleanOption("silent", false); }

  /// @brief maximum number of plans to produce
  size_t maxNumberOfPlans() const {
    double value = getNumericOption("maxNumberOfPlans", 0.0);
    if (value > 0.0) {
      return static_cast<size_t>(value);
    }
    return 0;
  }

  /// @brief add a node to the list of nodes
  void addNode(AstNode* node) { _resources.addNode(node); }

  /// @brief register a string
  /// the string is freed when the query is destroyed
  char* registerString(char const* p, size_t length) { return _resources.registerString(p, length); }

  /// @brief register a string
  /// the string is freed when the query is destroyed
  char* registerString(std::string const& value) { return _resources.registerString(value); }

  /// @brief register a potentially UTF-8-escaped string
  /// the string is freed when the query is destroyed
  char* registerEscapedString(char const* p, size_t length, size_t& outLength) { 
    return _resources.registerEscapedString(p, length, outLength); 
  }
  
  /// @brief memory limit for query
  size_t memoryLimit() const {
    double value = getNumericOption("memoryLimit", 0.0);
    if (value > 0.0) {
      return static_cast<size_t>(value);
    }
    return 0;
  }

  /// @brief maximum number of plans to produce
  int64_t literalSizeThreshold() const {
    double value = getNumericOption("literalSizeThreshold", 0.0);
    if (value > 0.0) {
      return static_cast<int64_t>(value);
    }
    return -1;
  }

  /// @brief extract a region from the query
  std::string extractRegion(int, int) const;

  /// @brief register an error, with an optional parameter inserted into printf
  /// this also makes the query abort
  void registerError(int, char const* = nullptr);

  /// @brief register an error with a custom error message
  /// this also makes the query abort
  void registerErrorCustom(int, char const*);

  /// @brief register a warning
  void registerWarning(int, char const* = nullptr);

  /// @brief prepare an AQL query, this is a preparation for execute, but
  /// execute calls it internally. The purpose of this separate method is
  /// to be able to only prepare a query from VelocyPack and then store it in the
  /// QueryRegistry.
  QueryResult prepare(QueryRegistry*);

  /// @brief execute an AQL query
  QueryResult execute(QueryRegistry*);

  /// @brief execute an AQL query
  /// may only be called with an active V8 handle scope
  QueryResultV8 executeV8(v8::Isolate* isolate, QueryRegistry*);

  /// @brief parse an AQL query
  QueryResult parse();

  /// @brief explain an AQL query
  QueryResult explain();

  /// @brief get v8 executor
  Executor* executor();

  /// @brief return the engine, if prepared
  ExecutionEngine* engine() { return _engine; }

  /// @brief inject the engine
  void engine(ExecutionEngine* engine) { _engine = engine; }

  /// @brief return the transaction, if prepared
  inline arangodb::Transaction* trx() { return _trx; }

  /// @brief get the plan for the query
  ExecutionPlan* plan() const { return _plan; }

  /// @brief whether or not the query returns verbose error messages
  bool verboseErrors() const {
    return getBooleanOption("verboseErrors", false);
  }

  /// @brief set the plan for the query
  void setPlan(ExecutionPlan* plan);

  /// @brief enter a V8 context
  void enterContext();

  /// @brief exits a V8 context
  void exitContext();

  /// @brief returns statistics for current query.
  void getStats(arangodb::velocypack::Builder&);

  /// @brief fetch a boolean value from the options
  bool getBooleanOption(char const*, bool) const;

  /// @brief add the list of warnings to VelocyPack.
  ///        Will add a new entry { ..., warnings: <warnings>, } if there are
  ///        warnings. If there are none it will not modify the builder
   void addWarningsToVelocyPackObject(arangodb::velocypack::Builder&) const;

  /// @brief transform the list of warnings to VelocyPack.
  ///        NOTE: returns nullptr if there are no warnings.
   std::shared_ptr<arangodb::velocypack::Builder> warningsToVelocyPack() const;

  /// @brief fetch the global query tracking value
  static bool DisableQueryTracking() { return DoDisableQueryTracking; }
  
  /// @brief turn off tracking globally
  static void DisableQueryTracking(bool value) {
    DoDisableQueryTracking = value;
  }
  
  /// @brief fetch the global slow query threshold value
  static double SlowQueryThreshold() { return SlowQueryThresholdValue; }
  
  /// @brief set global slow query threshold value
  static void SlowQueryThreshold(double value) {
    SlowQueryThresholdValue = value;
  }

  /// @brief get a description of the query's current state
  std::string getStateString() const;

  /// @brief look up a graph in the _graphs collection
  Graph const* lookupGraphByName(std::string const& name);

 private:
  /// @brief initializes the query
  void init();
  
  void setExecutionTime();

  /// @brief log a query
  void log();

  /// @brief calculate a hash value for the query and bind parameters
  uint64_t hash() const;

  /// @brief whether or not the query cache can be used for the query
  bool canUseQueryCache() const;

  /// @brief fetch a numeric value from the options
 public:
  double getNumericOption(char const*, double) const;

 private:
  /// @brief read the "optimizer.inspectSimplePlans" section from the options
  bool inspectSimplePlans() const;

  /// @brief read the "optimizer.rules" section from the options
  std::vector<std::string> getRulesFromOptions() const;

  /// @brief neatly format transaction errors to the user.
  QueryResult transactionError(int errorCode) const;

  /// @brief enter a new state
  void enterState(ExecutionState);

  /// @brief cleanup plan and engine for current query
  void cleanupPlanAndEngine(int);

  /// @brief create a TransactionContext
  std::shared_ptr<arangodb::TransactionContext> createTransactionContext();

  /// @brief returns the next query id
  static TRI_voc_tick_t NextId();

 private:
  /// @brief query id
  TRI_voc_tick_t _id;
  
  /// @brief current resources and limits used by query
  ResourceMonitor _resourceMonitor;
  
  /// @brief resources used by query
  QueryResources _resources;

  /// @brief pointer to vocbase the query runs in
  TRI_vocbase_t* _vocbase;

  /// @brief V8 code executor
  Executor* _executor;

  /// @brief the currently used V8 context
  V8Context* _context;

  /// @brief warnings collected during execution
  std::unordered_map<std::string, Graph*> _graphs;
  
  /// @brief the actual query string
  char const* _queryString;

  /// @brief length of the query string in bytes
  size_t const _queryLength;

  /// @brief query in a VelocyPack structure
  std::shared_ptr<arangodb::velocypack::Builder> const _queryBuilder;

  /// @brief bind parameters for the query
  BindParameters _bindParameters;

  /// @brief query options
  std::shared_ptr<arangodb::velocypack::Builder> _options;

  /// @brief collections used in the query
  Collections _collections;
  
  /// @brief _ast, we need an ast to manage the memory for AstNodes, even
  /// if we do not have a parser, because AstNodes occur in plans and engines
  Ast* _ast;

  /// @brief query execution profile
  Profile* _profile;

  /// @brief current state the query is in (used for profiling and error
  /// messages)
  ExecutionState _state;

  /// @brief the ExecutionPlan object, if the query is prepared
  ExecutionPlan* _plan;

  /// @brief the Parser object, if the query is prepared
  Parser* _parser;

  /// @brief the transaction object, in a distributed query every part of
  /// the query has its own transaction object. The transaction object is
  /// created in the prepare method.
  arangodb::Transaction* _trx;

  /// @brief the ExecutionEngine object, if the query is prepared
  ExecutionEngine* _engine;

  /// @brief maximum number of warnings
  size_t _maxWarningCount;

  /// @brief warnings collected during execution
  std::vector<std::pair<int, std::string>> _warnings;
 
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

  /// @brief global threshold value for slow queries
  static double SlowQueryThresholdValue;

  /// @brief whether or not query tracking is disabled globally
  static bool DoDisableQueryTracking;
};
}
}

#endif
