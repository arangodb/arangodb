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

#include "Basics/Common.h"
#include "Basics/ReadWriteLock.h"
#include "Aql/types.h"
#include "Cluster/ResultT.h"

struct TRI_vocbase_t;

namespace arangodb {
namespace aql {
class ExecutionEngine;
class Query;

class QueryRegistry {
 public:
  explicit QueryRegistry(double defTTL) : _defaultTTL(defTTL) {}

  TEST_VIRTUAL ~QueryRegistry();
  
public:

  /// @brief insert, this inserts the query <query> for the vocbase <vocbase>
  /// and the id <id> into the registry. It is in error if there is already
  /// a query for this <vocbase> and <id> combination and an exception will
  /// be thrown in that case. The time to live <ttl> is in seconds and the
  /// query will be deleted if it is not opened for that amount of time.
  /// With keepLease == true the query will be kept open and it is guaranteed
  /// that the caller can continue to use it exclusively.
  /// This is identical to an atomic sequence of insert();open();
  TEST_VIRTUAL void insert(QueryId id, Query* query, double ttl, bool isPrepare, bool keepLease);

  /// @brief open, find a query in the registry, if none is found, a nullptr
  /// is returned, otherwise, ownership of the query is transferred to the
  /// caller, however, the registry retains the entry and will open will
  /// succeed only once. If an already open query with the given id is
  /// found, an exception is thrown.
  /// An open query can directly be destroyed by the destroy method.
  /// Note that an open query will not expire, so users should please
  /// protect against leaks. If an already open query is found, an exception
  /// is thrown.
  Query* open(TRI_vocbase_t* vocbase, QueryId id);

  /// @brief close, return a query to the registry, if the query is not found,
  /// an exception is thrown. If the ttl is negative (the default is), the
  /// original ttl is taken again.
  void close(TRI_vocbase_t* vocbase, QueryId id, double ttl = -1.0);

  /// @brief destroy, this removes the entry from the registry and calls
  /// delete on the Query*. It is allowed to call this regardless of whether
  /// the query is open or closed. No check is performed that this call comes
  /// from the same thread that has opened it! Note that if the query is
  /// "open", then this will set the "killed" flag in the query and do not
  /// more.
  TEST_VIRTUAL void destroy(std::string const& vocbase, QueryId id, int errorCode);

  void destroy(TRI_vocbase_t* vocbase, QueryId id, int errorCode);

  ResultT<bool> isQueryInUse(TRI_vocbase_t* vocbase, QueryId id);

  /// @brief expireQueries, this deletes all expired queries from the registry
  void expireQueries();

  /// @brief return number of registered queries
  size_t numberRegisteredQueries();

  /// @brief for shutdown, we need to shut down all queries:
  void destroyAll();
  
  /// @brief return the default TTL value
  TEST_VIRTUAL double defaultTTL() const { return _defaultTTL; }

 private:

  /**
   * @brief Set the thread-local _noLockHeaders variable
   *
   * @param engine The Query engine that contains the no-lock-header
   *        information.
   */
  void setNoLockHeaders(ExecutionEngine* engine) const;

 private:
  /// @brief a struct for all information regarding one query in the registry
  struct QueryInfo {
    QueryInfo(QueryInfo const&) = delete;
    QueryInfo& operator=(QueryInfo const&) = delete;

    QueryInfo(QueryId id, Query* query, double ttl, bool isPrepared);
    ~QueryInfo();

    TRI_vocbase_t* _vocbase;  // the vocbase
    QueryId _id;              // id of the query
    Query* _query;            // the actual query pointer
    bool _isOpen;             // flag indicating whether or not the query
                              // is in use
    bool _isPrepared;
    double _timeToLive;       // in seconds
    double _expires;          // UNIX UTC timestamp of expiration
  };

  /// @brief _queries, the actual map of maps for the registry
  /// maps from vocbase name to list queries
  std::unordered_map<std::string, std::unordered_map<QueryId, std::unique_ptr<QueryInfo>>>
      _queries;

  /// @brief _lock, the read/write lock for access
  basics::ReadWriteLock _lock;
  
  /// @brief the default TTL value
  double const _defaultTTL;
};

}  // namespace arangodb::aql
}  // namespace arangodb

#endif
