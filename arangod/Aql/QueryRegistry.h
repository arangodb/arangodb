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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_QUERY_REGISTRY_H
#define ARANGOD_AQL_QUERY_REGISTRY_H 1

#include "Aql/types.h"
#include "Basics/Common.h"
#include "Basics/ReadWriteLock.h"
#include "Cluster/CallbackGuard.h"
#include "Cluster/ResultT.h"

#include <variant>

struct TRI_vocbase_t;

namespace arangodb {
namespace aql {
class ExecutionEngine;
class ClusterQuery;

/// manages cluster queries and engines
class QueryRegistry {
 public:
  explicit QueryRegistry(double defTTL) : _defaultTTL(defTTL), _disallowInserts(false) {}

  TEST_VIRTUAL ~QueryRegistry();

  /// @brief insert, this inserts the query <query> for the vocbase <vocbase>
  /// and the id <id> into the registry. It is in error if there is already
  /// a query for this <vocbase> and <id> combination and an exception will
  /// be thrown in that case. The time to live <ttl> is in seconds and the
  /// query will be deleted if it is not opened for that amount of time.
  /// With keepLease == true the query will be kept open and it is guaranteed
  /// that the caller can continue to use it exclusively.
  /// This is identical to an atomic sequence of insert();open();
  /// The callback guard needs to be stored with the query to prevent it from
  /// firing. This is used for the RebootTracker to destroy the query when
  /// the coordinator which created it restarts or fails.
  TEST_VIRTUAL void insertQuery(std::unique_ptr<ClusterQuery> query, double ttl, cluster::CallbackGuard guard);

  /// @brief requests a snippet from the query registry. if no snippet with
  /// such id exists, will return a nullptr. may also throw exceptions under
  /// conditions. please check the docs for openEngine, which is also called
  /// by this method.
  std::shared_ptr<ExecutionEngine> openExecutionEngine(EngineId eid);

  /// @brief requests a traverser engine from the query registry. if no engine
  /// with such id exists, will return a nullptr. may also throw exceptions under
  /// conditions. please check the docs for openEngine, which is also called
  /// by this method.
  std::shared_ptr<traverser::BaseEngine> openGraphEngine(EngineId eid);

  /// @brief close, return a query to the registry, if the query is not found,
  /// an exception is thrown. If the ttl is negative (the default is), the
  /// original ttl is taken again.
  void closeEngine(EngineId eId);

  /// @brief destroy, this removes the entry from the registry and calls
  /// delete on the Query*. It is allowed to call this regardless of whether
  /// the query is open or closed. No check is performed that this call comes
  /// from the same thread that has opened it! Note that if the query is
  /// "open", then this will set the "killed" flag in the query and do not
  /// more.
  /// if the ignoreOpened flag is set, it means the query will be shut down
  /// and removed regardless if it is in use by anything else. this is only
  /// safe to call if the current thread is currently using the query itself
  // cppcheck-suppress virtualCallInConstructor
  std::unique_ptr<ClusterQuery> destroyQuery(std::string const& vocbase, QueryId qId,
                                             int errorCode);
  
  /// used for a legacy shutdown
  bool destroyEngine(EngineId qId, int errorCode);

  /// @brief destroy all queries for the specified database. this can be used
  /// when the database gets dropped  
  void destroy(std::string const& vocbase);

  /// @brief expireQueries, this deletes all expired queries from the registry
  void expireQueries();

  /// @brief return number of registered queries
  size_t numberRegisteredQueries();

  /// @brief for shutdown, we need to shut down all queries:
  void destroyAll();

  /// @brief from here on, disallow entering new queries into the registry
  void disallowInserts();
  
  /// @brief registers query snippets in the registry. only use on coordinator
  void registerSnippets(SnippetList const&);
  
  /// @brief unregister query snippets from the registry.
  void unregisterSnippets(SnippetList const&) noexcept;

  /// @brief return the default TTL value
  TEST_VIRTUAL double defaultTTL() const { return _defaultTTL; }

 private:
  /// @brief open, find a engine in the registry, if none is found, a nullptr
  /// is returned, otherwise a shared_ptr is returned to the caller.
  /// the query registry retains the entry, and open will succeed only once. 
  /// If an already open query with the given id is found, an exception is thrown.
  /// An open query can be destroyed by the destroy method.
  /// Note that an open query will not expire, so users should please
  /// protect against leaks. If an already open query is found, an exception
  /// is thrown.
  /// note: supported return types here are:
  /// - std::shared_ptr<ExecutionEngine> for query snippets 
  /// - std::shared_ptr<traverser::BaseEngine> for traverser engines
  template<typename T>
  std::shared_ptr<T> openEngine(EngineId id);
  
  /// @brief a struct for all information regarding one query in the registry
  struct QueryInfo final {
    QueryInfo(std::unique_ptr<ClusterQuery> query, double ttl, cluster::CallbackGuard guard);
    ~QueryInfo();

    TRI_vocbase_t* _vocbase;  // the vocbase
    std::unique_ptr<ClusterQuery> _query;  // the actual query pointer
    
    double const _timeToLive;  // in seconds
    double _expires;     // UNIX UTC timestamp of expiration
    size_t _numEngines; // used for legacy shutdown
    size_t _numOpen;

    cluster::CallbackGuard _rebootTrackerCallbackGuard;
  };

  struct EngineInfo final {
    EngineInfo(EngineInfo const&) = delete;
    EngineInfo& operator=(EngineInfo const&) = delete;
    
    EngineInfo(EngineInfo&& other)
      : _engine(std::move(other._engine)),
        _queryInfo(std::move(other._queryInfo)),
        _isOpen(other._isOpen) {}
    EngineInfo& operator=(EngineInfo&& other) = delete;
    
    EngineInfo(std::shared_ptr<ExecutionEngine> en, QueryInfo* qi)
      : _engine(std::move(en)), _queryInfo(qi), _isOpen(false) {}
    
    EngineInfo(std::shared_ptr<traverser::BaseEngine> en, QueryInfo* qi)
      : _engine(en), _queryInfo(qi), _isOpen(false) {}
  
    /// @brief we store either a shared_ptr to an ExecutionEngine or a 
    /// shared_ptr to a traverser::BaseEngine. we don't need to mess with
    /// unions and a type info here, as std::variant will do all the hard
    /// work for us
    using EngineType = std::variant<
        std::shared_ptr<ExecutionEngine>, 
        std::shared_ptr<traverser::BaseEngine>>;
  
    EngineType _engine;
    QueryInfo* _queryInfo;
    bool _isOpen;
  };
  
  /// @brief _queries, the actual map of maps for the registry
  /// maps from vocbase name to list of queries
  std::unordered_map<std::string, std::unordered_map<QueryId, std::unique_ptr<QueryInfo>>> _queries;
  
  std::unordered_map<EngineId, EngineInfo> _engines;

  /// @brief _lock, the read/write lock for access
  basics::ReadWriteLock _lock;

  /// @brief the default TTL value
  double const _defaultTTL;

  bool _disallowInserts;
};

}  // namespace aql
}  // namespace arangodb

#endif
