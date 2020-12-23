////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_AQL_QUERY_CONTEXT_H
#define ARANGOD_AQL_QUERY_CONTEXT_H 1

#include "Aql/Collections.h"
#include "Aql/Graphs.h"
#include "Aql/QueryExecutionState.h"
#include "Aql/QueryOptions.h"
#include "Aql/QueryString.h"
#include "Aql/QueryWarnings.h"
#include "Aql/types.h"
#include "Basics/Common.h"
#include "Basics/ResourceUsage.h"
#include "Cluster/ResultT.h"
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

class Ast;

/// @brief an AQL query basic interface
class QueryContext {
 private:

  QueryContext(QueryContext const&) = delete;
  QueryContext& operator=(QueryContext const&) = delete;

 public:
  
  explicit QueryContext(TRI_vocbase_t& vocbase);

  virtual ~QueryContext();

  arangodb::ResourceMonitor& resourceMonitor() noexcept { return _resourceMonitor; }
  
  /// @brief get the vocbase
  inline TRI_vocbase_t& vocbase() const { return _vocbase; }
  
  Collections& collections();
  Collections const& collections() const;

  /// @brief return the names of collections used in the query
  std::vector<std::string> collectionNames() const;

  /// @brief return the user that started the query
  virtual std::string const& user() const;
  
  /// warnings access is thread safe
  QueryWarnings& warnings() { return _warnings; }

  /// @brief look up a graph in the _graphs collection
  ResultT<graph::Graph const*> lookupGraphByName(std::string const& name);
  
  /// @brief note that the query uses the DataSource
  void addDataSource(std::shared_ptr<arangodb::LogicalDataSource> const& ds);

  QueryExecutionState::ValueType state() const { return _execState; }
  
  TRI_voc_tick_t id() const { return _queryId; }
  
  aql::Ast* ast();

  void incHttpRequests(unsigned i) {
    _numRequests.fetch_add(i, std::memory_order_relaxed);
  }
      
 public:
  
  virtual QueryOptions const& queryOptions() const = 0;
  
  /// @brief pass-thru a resolver object from the transaction context
  virtual CollectionNameResolver const& resolver() const = 0;
  
  virtual velocypack::Options const& vpackOptions() const = 0;
  
  /// @brief create a transaction::Context
  virtual std::shared_ptr<transaction::Context> newTrxContext() const = 0;
  
  virtual transaction::Methods& trxForOptimization() = 0;
    
  virtual bool killed() const = 0;

  /// @brief whether or not a query is a modification query
  virtual bool isModificationQuery() const noexcept = 0;
  virtual bool isAsyncQuery() const noexcept = 0;

  virtual double getLockTimeout() const noexcept = 0;
  virtual void setLockTimeout(double timeout) = 0;
  
 public:
  
  virtual void enterV8Context();
  
  virtual void exitV8Context() {}
  
  virtual bool hasEnteredV8Context() const { return false; }

 protected:
  
  TRI_voc_tick_t const _queryId;

  /// @brief current resources and limits used by query
  arangodb::ResourceMonitor _resourceMonitor;
  
  /// @brief thread-safe query warnings collector
  QueryWarnings _warnings;
  
  /// @brief collections used in the query
  Collections _collections;

  /// @brief pointer to vocbase the query runs in
  TRI_vocbase_t& _vocbase;
  
  /// @brief graphs used in query, identified by name
  std::unordered_map<std::string, std::unique_ptr<graph::Graph>> _graphs;

  /// @brief set of DataSources used in the query
  ///        needed for the query cache, stores datasource guid -> datasource name
  std::unordered_map<std::string, std::string> _queryDataSources;
  
  /// @brief current state the query is in (used for profiling and error messages)
  QueryExecutionState::ValueType _execState;
  
  /// @brief _ast, we need an ast to manage the memory for AstNodes, even
  /// if we do not have a parser, because AstNodes occur in plans and engines
  std::unique_ptr<Ast> _ast;
  
  std::atomic<unsigned> _numRequests;
};

}  // namespace aql
}  // namespace arangodb

#endif
