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

#ifndef ARANGOD_AQL_QUERY_CACHE_H
#define ARANGOD_AQL_QUERY_CACHE_H 1

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Aql/QueryString.h"
#include "Basics/Common.h"
#include "Basics/Mutex.h"
#include "Basics/ReadWriteLock.h"

struct TRI_vocbase_t;

namespace arangodb {

class LogicalDataSource; // forward declaration

}

namespace arangodb {
namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack
namespace aql {

/// @brief cache mode
enum QueryCacheMode { CACHE_ALWAYS_OFF, CACHE_ALWAYS_ON, CACHE_ON_DEMAND };

struct QueryCacheProperties {
  QueryCacheMode mode;
  uint64_t maxResultsCount;
  uint64_t maxResultsSize;
  uint64_t maxEntrySize;
  bool includeSystem;
  bool showBindVars;
};

struct QueryCacheResultEntry {
  QueryCacheResultEntry() = delete;

  QueryCacheResultEntry(uint64_t hash, QueryString const& queryString,
                        std::shared_ptr<arangodb::velocypack::Builder> const& queryResult,
                        std::shared_ptr<arangodb::velocypack::Builder> const& bindVars,
                        std::unordered_map<std::string, std::string>&& dataSources
  );

  ~QueryCacheResultEntry() = default;

  uint64_t const _hash;
  std::string const _queryString;
  std::shared_ptr<arangodb::velocypack::Builder> const _queryResult;
  std::shared_ptr<arangodb::velocypack::Builder> const _bindVars;
  // stores datasource guid -> datasource name
  std::unordered_map<std::string, std::string> const _dataSources;
  std::shared_ptr<arangodb::velocypack::Builder> _stats;
  size_t _size;
  size_t _rows;
  std::atomic<uint64_t> _hits;
  double _stamp;
  QueryCacheResultEntry* _prev;
  QueryCacheResultEntry* _next;

  void increaseHits() { _hits.fetch_add(1, std::memory_order_relaxed); }
  double executionTime() const;

  void toVelocyPack(arangodb::velocypack::Builder& builder) const;
  
  /// current user has all permissions
  bool currentUserHasPermissions() const;
};

struct QueryCacheDatabaseEntry {
  QueryCacheDatabaseEntry(QueryCacheDatabaseEntry const&) = delete;
  QueryCacheDatabaseEntry& operator=(QueryCacheDatabaseEntry const&) = delete;

  /// @brief create a database-specific cache
  QueryCacheDatabaseEntry();

  /// @brief destroy a database-specific cache
  ~QueryCacheDatabaseEntry();

  /// @brief lookup a query result in the database-specific cache
  std::shared_ptr<QueryCacheResultEntry> lookup(
      uint64_t hash, QueryString const& queryString,
      std::shared_ptr<arangodb::velocypack::Builder> const& bindVars) const;

  /// @brief store a query result in the database-specific cache
  void store(std::shared_ptr<QueryCacheResultEntry>&& entry,
             size_t allowedMaxResultsCount, size_t allowedMaxResultsSize);

  /// @brief invalidate all entries for the given data sources in the
  /// database-specific cache
  void invalidate(std::vector<std::string> const& dataSourceGuids);

  /// @brief invalidate all entries for a data source in the
  /// database-specific cache
  void invalidate(std::string const& dataSourceGuid);

  void queriesToVelocyPack(arangodb::velocypack::Builder& builder) const;

  /// @brief enforce maximum number of results
  /// must be called under the shard's lock
  void enforceMaxResults(size_t numResults, size_t sizeResults);

  /// @brief enforce maximum size of individual entries
  /// must be called under the shard's lock
  void enforceMaxEntrySize(size_t value);

  /// @brief exclude all data from system collections
  /// must be called under the shard's lock
  void excludeSystem();

  /// @brief unlink the result entry from all datasource maps
  void removeDatasources(QueryCacheResultEntry const* e);

  /// @brief unlink the result entry from the list
  void unlink(QueryCacheResultEntry*);

  /// @brief link the result entry to the end of the list
  void link(QueryCacheResultEntry*);

  /// @brief hash table that maps query hashes to query results
  std::unordered_map<uint64_t, std::shared_ptr<QueryCacheResultEntry>> _entriesByHash;

  /// @brief hash table that contains all data souce-specific query results
  ///        maps from data sources GUIDs to a set of query results as defined in
  /// _entriesByHash
  std::unordered_map<std::string, std::pair<bool, std::unordered_set<uint64_t>>> _entriesByDataSourceGuid;

  /// @brief beginning of linked list of result entries
  QueryCacheResultEntry* _head;

  /// @brief end of linked list of result entries
  QueryCacheResultEntry* _tail;

  /// @brief number of results in this cache
  size_t _numResults;

  /// @brief total size of results in this cache
  size_t _sizeResults;
};

class QueryCache {
 public:
  QueryCache(QueryCache const&) = delete;
  QueryCache& operator=(QueryCache const&) = delete;

  /// @brief create cache
  QueryCache();

  /// @brief destroy the cache
  ~QueryCache();

 public:
  /// @brief return the query cache properties
  QueryCacheProperties properties() const;

  /// @brief sets the cache properties
  void properties(QueryCacheProperties const& properties);

  /// @brief sets the cache properties
  void properties(arangodb::velocypack::Slice const& properties);

  /// @brief return the query cache properties
  void toVelocyPack(arangodb::velocypack::Builder& builder) const;

  /// @brief test whether the cache might be active
  /// this is a quick test that may save the caller from further bothering
  /// about the query cache if case it returns `false`
  bool mayBeActive() const;

  /// @brief return whether or not the query cache is enabled
  QueryCacheMode mode() const;

  /// @brief return a string version of the mode
  static std::string modeString(QueryCacheMode);

  /// @brief return the internal type for a mode string
  static QueryCacheMode modeString(std::string const&);

  /// @brief lookup a query result in the cache
  std::shared_ptr<QueryCacheResultEntry> lookup(
      TRI_vocbase_t* vocbase, uint64_t hash, QueryString const& queryString,
      std::shared_ptr<arangodb::velocypack::Builder> const& bindVars) const;

  /// @brief store a query cache entry in the cache
  void store(TRI_vocbase_t* vocbase, std::shared_ptr<QueryCacheResultEntry> entry);

  /// @brief invalidate all queries for the given data sources
  void invalidate(TRI_vocbase_t* vocbase, std::vector<std::string> const& dataSourceGuids);

  /// @brief invalidate all queries for a particular data source
  void invalidate(TRI_vocbase_t* vocbase, std::string const& dataSourceGuid);

  /// @brief invalidate all queries for a particular database
  void invalidate(TRI_vocbase_t* vocbase);

  /// @brief invalidate all queries
  void invalidate();

  /// @brief get the pointer to the global query cache
  static QueryCache* instance();

  /// @brief create a velocypack representation of the queries in the cache
  void queriesToVelocyPack(TRI_vocbase_t* vocbase, arangodb::velocypack::Builder& builder) const;

 private:
  /// @brief invalidate all entries in the cache part
  /// note that the caller of this method must hold the write lock
  void invalidate(unsigned int part);

  /// @brief enforce maximum number of results in each database-specific cache
  /// must be called under the cache's properties lock
  void enforceMaxResults(size_t numResults, size_t sizeResults);

  /// @brief enforce maximum size of individual entries in each
  /// database-specific cache must be called under the cache's properties lock
  void enforceMaxEntrySize(size_t value);

  /// @brief exclude all data from system collections
  void excludeSystem();

  /// @brief sets the maximum number of elements in the cache
  void setMaxResults(size_t numResults, size_t sizeResults);

  /// @brief sets the maximum size for each entry in the cache
  void setMaxEntrySize(size_t value);

  /// @brief sets the "include-system" flag
  void setIncludeSystem(bool value);

  /// @brief enable or disable the query cache
  void setMode(QueryCacheMode);

  /// @brief determine which part of the cache to use for the cache entries
  unsigned int getPart(TRI_vocbase_t const*) const;

 private:
  /// @brief number of R/W locks for the query cache
  static constexpr uint64_t numberOfParts = 16;

  /// @brief protect mode changes with a mutex
  mutable arangodb::Mutex _propertiesLock;

  /// @brief read-write lock for the cache
  mutable arangodb::basics::ReadWriteLock _entriesLock[numberOfParts];

  /// @brief cached query entries, organized per database
  std::unordered_map<TRI_vocbase_t*, std::unique_ptr<QueryCacheDatabaseEntry>> _entries[numberOfParts];
};
}  // namespace aql
}  // namespace arangodb

#endif
