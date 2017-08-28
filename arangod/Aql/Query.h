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
#include "Aql/QueryExecutionState.h"
#include "Aql/QueryOptions.h"
#include "Aql/QueryResources.h"
#include "Aql/QueryResultV8.h"
#include "Aql/QueryString.h"
#include "Aql/RegexCache.h"
#include "Aql/ResourceUsage.h"
#include "Aql/types.h"
#include "Basics/Common.h"
#include "V8Server/V8Context.h"
#include "VocBase/voc-types.h"

struct TRI_vocbase_t;

namespace arangodb {

namespace transaction {
class Context;
class Methods;
}

namespace velocypack {
class Builder;
}

namespace aql {

struct AstNode;
class Ast;
class ExecutionEngine;
class ExecutionPlan;
class Query;
struct QueryProfile;
class QueryRegistry;
class V8Executor;

/// @brief equery part
enum QueryPart { PART_MAIN, PART_DEPENDENT };

/// @brief an AQL query
class Query {
 private:
  Query(Query const&) = delete;
  Query& operator=(Query const&) = delete;

 public:
  Query(bool contextOwnedByExterior, TRI_vocbase_t*, QueryString const& queryString,
        std::shared_ptr<arangodb::velocypack::Builder> const& bindParameters,
        std::shared_ptr<arangodb::velocypack::Builder> const& options, QueryPart);

  Query(bool contextOwnedByExterior, TRI_vocbase_t*,
        std::shared_ptr<arangodb::velocypack::Builder> const& queryStruct,
        std::shared_ptr<arangodb::velocypack::Builder> const& options, QueryPart);

  ~Query();

  /// @brief clone a query
  /// note: as a side-effect, this will also create and start a transaction for
  /// the query
  Query* clone(QueryPart, bool);

 public:

  QueryString const& queryString() const { return _queryString; }

  /// @brief Inject a transaction from outside. Use with care!
  void injectTransaction (transaction::Methods* trx) {
    _trx = trx;
    init();
  }
  
  QueryProfile* profile() const {
    return _profile.get();
  }

  QueryOptions const& queryOptions() const { return _queryOptions; }

  void increaseMemoryUsage(size_t value) { _resourceMonitor.increaseMemoryUsage(value); }
  void decreaseMemoryUsage(size_t value) { _resourceMonitor.decreaseMemoryUsage(value); }
  
  ResourceMonitor* resourceMonitor() { return &_resourceMonitor; }

  /// @brief return the start timestamp of the query
  double startTime() const { return _startTime; }
  
  /// @brief return the current runtime of the query
  double runTime(double now) const { return now - _startTime; }
  
  /// @brief return the current runtime of the query
  double runTime() const { return runTime(TRI_microtime()); }

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

  /// @brief getter for _ast
  Ast* ast() const { 
    return _ast.get(); 
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
  
  /// @brief register an error, with an optional parameter inserted into printf
  /// this also makes the query abort
  void registerError(int, char const* = nullptr);

  /// @brief register an error with a custom error message
  /// this also makes the query abort
  void registerErrorCustom(int, char const*);

  /// @brief register a warning
  void registerWarning(int, char const* = nullptr);
  
  void prepare(QueryRegistry*, uint64_t queryHash);

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
  V8Executor* executor();
  
  /// @brief cache for regular expressions constructed by the query
  RegexCache* regexCache() { return &_regexCache; }

  /// @brief return the engine, if prepared
  ExecutionEngine* engine() const { return _engine.get(); }

  /// @brief inject the engine
  void engine(ExecutionEngine* engine);

  /// @brief return the transaction, if prepared
  inline transaction::Methods* trx() { return _trx; }

  /// @brief get the plan for the query
  ExecutionPlan* plan() const { return _plan.get(); }

  /// @brief enter a V8 context
  void enterContext();

  /// @brief exits a V8 context
  void exitContext();

  /// @brief returns statistics for current query.
  void getStats(arangodb::velocypack::Builder&);

  /// @brief add the list of warnings to VelocyPack.
  ///        Will add a new entry { ..., warnings: <warnings>, } if there are
  ///        warnings. If there are none it will not modify the builder
  void addWarningsToVelocyPackObject(arangodb::velocypack::Builder&) const;

  /// @brief transform the list of warnings to VelocyPack.
  ///        NOTE: returns nullptr if there are no warnings.
  std::shared_ptr<arangodb::velocypack::Builder> warningsToVelocyPack() const;
  
  /// @brief get a description of the query's current state
  std::string getStateString() const;

  /// @brief look up a graph in the _graphs collection
  Graph const* lookupGraphByName(std::string const& name);

  /// @brief return the bind parameters as passed by the user
  std::shared_ptr<arangodb::velocypack::Builder> bindParameters() const { 
    return _bindParameters.builder(); 
  }
 
  QueryExecutionState::ValueType state() const { return _state; }

 private:
  /// @brief initializes the query
  void init();
  
  /// @brief prepare an AQL query, this is a preparation for execute, but
  /// execute calls it internally. The purpose of this separate method is
  /// to be able to only prepare a query from VelocyPack and then store it in the
  /// QueryRegistry.
  ExecutionPlan* prepare();

  void setExecutionTime();

  /// @brief log a query
  void log();

  /// @brief calculate a hash value for the query and bind parameters
  uint64_t hash();

  /// @brief whether or not the query cache can be used for the query
  bool canUseQueryCache() const;

 private:
  /// @brief neatly format exception messages for the users
  std::string buildErrorMessage(int errorCode) const;

  /// @brief enter a new state
  void enterState(QueryExecutionState::ValueType);

  /// @brief cleanup plan and engine for current query
  void cleanupPlanAndEngine(int, VPackBuilder* statsBuilder = nullptr);

  /// @brief create a transaction::Context
  std::shared_ptr<transaction::Context> createTransactionContext();

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
  std::unique_ptr<V8Executor> _executor;

  /// @brief the currently used V8 context
  V8Context* _context;

  /// @brief graphs used in query, identified by name
  std::unordered_map<std::string, Graph*> _graphs;
  
  /// @brief the actual query string
  QueryString _queryString;

  /// @brief query in a VelocyPack structure
  std::shared_ptr<arangodb::velocypack::Builder> const _queryBuilder;

  /// @brief bind parameters for the query
  BindParameters _bindParameters;

  /// @brief query options
  std::shared_ptr<arangodb::velocypack::Builder> _options;

  /// @brief query options
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
  transaction::Methods* _trx;

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
};
}
}

#endif
