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

#include <velocypack/Buffer.h>

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
// collection name bind parameters only.
// these bind parameters are relevant during the query optimization phase,
// because they determine which collections and indexes can be used by the
// query. once an AQL query is started that has its "optimizePlanForCaching"
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
  struct Key {
    QueryString queryString;

    // collection and attribute name bind parameters.
    // these parameters do not include value bind parameters
    std::shared_ptr<velocypack::Builder> bindParameters;

    // fullcount enabled for query: yes or no
    bool fullCount;

    size_t hash() const;
  };

  struct KeyHasher {
    size_t operator()(Key const& key) const noexcept;
  };

  struct KeyEqual {
    bool operator()(Key const& lhs, Key const& rhs) const noexcept;
  };

  struct Value {
    // list of all data sources (collections & views) used in the query.
    // needed so that we can invalidate the cache if the definition of a
    // data source changes or a data source gets dropped
    std::vector<std::string> dataSourceGuids;

    // plan velocypack
    std::shared_ptr<velocypack::UInt8Buffer> serializedPlan;

    // timestamp when this plan was cached
    double dateCreated;
  };

  struct Entry {
    Key key;
    Value value;
  };

  QueryPlanCache(QueryPlanCache const&) = delete;
  QueryPlanCache& operator=(QueryPlanCache const&) = delete;

  QueryPlanCache();

  ~QueryPlanCache();

  std::shared_ptr<velocypack::UInt8Buffer> lookup(Key const& key) const;

  void store(Key&& key, std::vector<std::string>&& dataSourceGuids,
             std::shared_ptr<velocypack::UInt8Buffer> serializedPlan);

  Key createCacheKey(QueryString const& queryString,
                     std::shared_ptr<velocypack::Builder> const& bindVars,
                     QueryOptions const& queryOptions) const;

  void invalidate(std::string_view dataSourceGuid);

  void invalidateAll();

  void toVelocyPack(velocypack::Builder& builder) const;

  static std::shared_ptr<velocypack::Builder> filterBindParameters(
      std::shared_ptr<velocypack::Builder> const& source);

 private:
  // mutex protecting _entries
  mutable std::shared_mutex _mutex;

  // mapping from plan cache key to stored plan velocypack
  std::unordered_map<Key, Value, KeyHasher, KeyEqual> _entries;
};

}  // namespace arangodb::aql
