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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/QueryString.h"
#include "Auth/Common.h"
#include "Metrics/Fwd.h"

#include <velocypack/Buffer.h>

#include <atomic>
#include <functional>
#include <memory>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace arangodb::velocypack {
class Builder;
}  // namespace arangodb::velocypack

namespace arangodb::aql {
struct QueryOptions;

// a very simple, not yet optimized query plan cache.
// the cache can cache serialized query execution plans (in velocypack format)
// in a map, mapping from {query string, bind parameters, fullCount} to the
// cached plan.
// the cache keys consist of the full query string, the query's `fullcount`
// attribute value and the set of "relevant" bind parameters, which are the
// collection name parameters only. attribute name bind parameters and value
// bind parameters are not part of the cache keys.
// the collection name bind parameters are relevant during the query
// optimization phase, because they determine which collections and indexes
// can be used by the query.
// once an AQL query is started that has its "optimizePlanForCaching"
// flag set to true, the Query object will generate a cache key object for the
// query plan cache.
// it will then perform a lookup in the query plan cache and run the query using
// the serialized cached plan if there is any in the cache. if there is no plan
// in the cache, the Query object will memorize its cache key and invoke the
// query parser and optimizer. after the optimization, the Query object will
// serialize the plan to velocypack and store the serialized plan in the plan
// cache, using the already computed cache key.
class QueryPlanCache {
 public:
  // maximum number of times a cached query plan is used before it is
  // invalidated and wiped from the cache.
  // the rationale for wiping entries from the cache after they have been
  // used several times is that somehow outdated entries get flush
  // eventually.
  static constexpr size_t kMaxNumUsages = 256;

  struct Key {
    QueryString queryString;

    // collection name bind parameters.
    // these parameters do not include value bind parameters and
    // attribute name bind parameters.
    // guaranteed to be a non-nullptr.
    std::shared_ptr<velocypack::UInt8Buffer> bindParameters;

    // fullcount enabled for query: yes or no
    bool fullCount;

    size_t hash() const;

    size_t memoryUsage() const noexcept;
  };

  struct KeyHasher {
    size_t operator()(Key const& key) const noexcept;
  };

  struct KeyEqual {
    bool operator()(Key const& lhs, Key const& rhs) const noexcept;
  };

  struct DataSourceEntry {
    // actual name of the datasource
    std::string name;

    // datasource access level. will either be read or write.
    auth::Level level;
  };

  struct Value {
    size_t memoryUsage() const noexcept;

    // list of all data sources (collections & views) used in the query.
    // needed so that we can invalidate the cache if the definition of a
    // data source changes or a data source gets dropped.
    // maps from datasource guid to a pair of {datasource name, datasource
    // access level}. access level is either read or write.
    std::unordered_map<std::string, DataSourceEntry> dataSources;

    // the query plan velocypack. guaranteed to be a non-nullptr.
    std::shared_ptr<velocypack::UInt8Buffer> serializedPlan;

    // timestamp when this plan was cached. currently not used, but
    // could be used when analyzing/exposing the contents of the query
    // plan cache later.
    double dateCreated;

    std::atomic<size_t> numUsed;
  };

  struct Entry {
    Key key;
    Value value;
  };

  QueryPlanCache(QueryPlanCache const&) = delete;
  QueryPlanCache& operator=(QueryPlanCache const&) = delete;

  QueryPlanCache(size_t maxEntries, size_t maxMemoryUsage,
                 size_t maxIndividualEntrySize,
                 metrics::Counter* numberOfHitsMetric,
                 metrics::Counter* numberOfMissesMetric);

  ~QueryPlanCache();

  // looks up a key in the cache.
  // returns the cache entry if it exists, and nullptr otherwise
  std::shared_ptr<Value const> lookup(Key const& key);

  // stores an entry in the cache.
  // returns true if the entry was stored successfully, and false if the entry
  // could not be stored (e.g. due to sizing constraints).
  bool store(Key&& key,
             std::unordered_map<std::string, DataSourceEntry>&& dataSources,
             std::shared_ptr<velocypack::UInt8Buffer> serializedPlan);

  Key createCacheKey(QueryString const& queryString,
                     std::shared_ptr<velocypack::Builder> const& bindVars,
                     QueryOptions const& queryOptions) const;

  void invalidate(std::string const& dataSourceGuid);

  void invalidateAll();

  void toVelocyPack(
      velocypack::Builder& builder,
      std::function<bool(QueryPlanCache::Key const&,
                         QueryPlanCache::Value const&)> const& filter) const;

  static std::shared_ptr<velocypack::UInt8Buffer> filterBindParameters(
      std::shared_ptr<velocypack::Builder> const& source);

 private:
  // apply sizing constraints (e.g. memory usage, max number of entries).
  // must be called while holding _mutex in exclusive mode.
  void applySizeConstraints();

  // mutex protecting _entries
  mutable std::shared_mutex _mutex;

  // mapping from plan cache key to stored plan velocypack.
  // protected by _mutex.
  std::unordered_map<Key, std::shared_ptr<Value>, KeyHasher, KeyEqual> _entries;

  // total approximate memory usage by _entries. protected by _mutex.
  size_t _memoryUsage;

  // maximum number of plan cache entries for the cache
  size_t const _maxEntries;

  // maximum total allowed memory usage for the cache.
  // note that this can be temporarily violated during insertion of a new
  // new cache entry.
  size_t const _maxMemoryUsage;

  // maximum allowed size for an individual entry.
  size_t const _maxIndividualEntrySize;

  /// @brief number of plan cache lookup hits
  metrics::Counter* _numberOfHitsMetric;

  /// @brief number of plan cache lookup misses
  metrics::Counter* _numberOfMissesMetric;
};

}  // namespace arangodb::aql
