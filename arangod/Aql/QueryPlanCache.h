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
#include <velocypack/SharedSlice.h>

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

// A very simple, not yet optimized query plan cache.
// The cache can cache serialized query execution plans (in velocypack format)
// in a map, mapping from {query string, bind parameters, fullCount} to the
// cached plan.
// The cache keys consist of the full query string, the query's
// `fullcount` and `forceOneShard` attribute values and the set of
// "relevant" bind parameters, which are the collection name parameters
// only. Attribute name bind parameters and value bind parameters are
// not part of the cache keys.
// We need all this in the key since they can influence query optimization!
// The collection name bind parameters are relevant during the query
// optimization phase, because they determine which collections and indexes
// can be used by the query.
// Once an AQL query is started that has its "optimizePlanForCaching"
// and "usePlanCache" flags set to true, the Query object will generate
// a cache key object for the query plan cache.
// It will then perform a lookup in the query plan cache and run the query using
// the serialized cached plan if there is any in the cache. If there is no plan
// in the cache, the Query object will memorize its cache key and invoke the
// query parser and optimizer. After the optimization, the Query object will
// serialize the plan to velocypack and store the serialized plan in the plan
// cache, using the already computed cache key.
class QueryPlanCache {
 public:
  struct VPackInBufferWithComparator {
    std::shared_ptr<velocypack::UInt8Buffer> buffer;
    VPackInBufferWithComparator(std::shared_ptr<velocypack::UInt8Buffer> buffer)
        : buffer(std::move(buffer)) {}
    bool operator==(VPackInBufferWithComparator const& other) const noexcept;
  };

  struct Key {
    QueryString queryString;

    // Collection name bind parameters.
    // These parameters do not include value bind parameters and
    // attribute name bind parameters.
    // Guaranteed to be a non-nullptr.
    VPackInBufferWithComparator bindParameters;

    // Fullcount enabled for query: yes or no
    bool fullCount;

    // forceOneShard enabled for query: yes or no
    bool forceOneShard;

    size_t hash() const;

    size_t memoryUsage() const noexcept;

    bool operator==(Key const& other) const noexcept = default;
  };

  struct KeyHasher {
    size_t operator()(Key const& key) const noexcept;
  };

  struct DataSourceEntry {
    // actual name of the datasource
    std::string name;

    // datasource access level. will either be read or write.
    auth::Level level;
  };

  struct Value {
    size_t memoryUsage() const noexcept;

    // List of all data sources (collections & views) used in the query.
    // Needed so that we can invalidate the cache if the definition of a
    // data source changes or a data source gets dropped.
    // Maps from datasource guid to a pair of {datasource name, datasource
    // access level}. Access level is either read or write.
    std::unordered_map<std::string, DataSourceEntry> dataSources;

    // The query plan velocypack. guaranteed to be a non-nullptr.
    velocypack::SharedSlice serializedPlan;

    // Timestamp when this plan was cached. currently not used, but
    // could be used when analyzing/exposing the contents of the query
    // plan cache later.
    double dateCreated;

    std::atomic<size_t> hits;
  };

  QueryPlanCache(QueryPlanCache const&) = delete;
  QueryPlanCache& operator=(QueryPlanCache const&) = delete;

  QueryPlanCache(size_t maxEntries, size_t maxMemoryUsage,
                 size_t maxIndividualEntrySize, double invalidationTime,
                 metrics::Counter* numberOfHitsMetric,
                 metrics::Counter* numberOfMissesMetric,
                 metrics::Gauge<uint64_t>* memoryUsageMetric);

  ~QueryPlanCache();

  // Looks up a key in the cache.
  // Returns the cache entry if it exists, and nullptr otherwise.
  std::shared_ptr<Value const> lookup(Key const& key);

  // Stores an entry in the cache.
  // Returns true if the entry was stored successfully, and false if the entry
  // could not be stored (e.g. due to sizing constraints).
  bool store(Key&& key,
             std::unordered_map<std::string, DataSourceEntry>&& dataSources,
             velocypack::SharedSlice serializedPlan);

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
  // Apply sizing constraints (e.g. memory usage, max number of entries).
  // must be called whilst holding _mutex in exclusive mode.
  void applySizeConstraints();

  // Mutex protecting _entries.
  mutable std::shared_mutex _mutex;

  // Mapping from plan cache key to stored plan velocypack.
  // protected by _mutex.
  std::unordered_map<Key, std::shared_ptr<Value>, KeyHasher> _entries;

  // Total approximate memory usage by _entries. protected by _mutex.
  size_t _memoryUsage;

  // Maximum number of plan cache entries for the cache.
  size_t const _maxEntries;

  // Maximum total allowed memory usage for the cache.
  // Note that this can be temporarily violated during insertion of a new
  // cache entry.
  size_t const _maxMemoryUsage;

  // Maximum allowed size for an individual entry.
  size_t const _maxIndividualEntrySize;

  // Time to invalidate a cache entry in seconds:
  double _invalidationTime;

  /// Number of plan cache lookup hits
  metrics::Counter* _numberOfHitsMetric;

  /// number of plan cache lookup misses
  metrics::Counter* _numberOfMissesMetric;

  /// Total memory usage across all QueryPlanCaches in all databases
  metrics::Gauge<uint64_t>* _totalMemoryUsageMetric;
};

}  // namespace arangodb::aql
