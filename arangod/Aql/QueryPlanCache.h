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

#include <memory>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace arangodb::velocypack {
class Builder;
class Slice;
}  // namespace arangodb::velocypack

namespace arangodb::aql {

// a very simple, not yet optimized query plan cache.
// the cache can cache serialized query execution plans (in velocypack format)
// in a map, mapping from {query string, bind parameters} to the cached plan.
// the cache keys consist of the full query string and the set of "relevant"
// bind parameters, which are the collection name bind parameters and the
// attribute name bind parameters. these bind parameters are relevant during the
// query optimization phase, because they determine which collections and
// indexes can be used by the query.
// once an AQL query is started that has its "optimizePlanForCaching" flag
// set to true, the Query object will generate a cache key object for the
// query plan cache.
// it will then perform a lookup in the query plan cache and run the query using
// the serialized cached plan if there is any in the cache. if there is no plan
// in the cache, the Query object will memorize its cache key and invoke the
// query parser and optimizer. after the optimization, the Query object will
// serialize the plan to velocypack and store the serialized plan in the plan
// cache, using the already computed cache key.
//
// TODO:
// - currently the cache is never invalidated, which is wrong. we must
// invalidate
//   the cache whenever
//   - a collection gets dropped
//   - maybe: the properties of a collection get changed (TODO: clarify if this
//     can influence the query plans at all)
//   - an index for a collection gets dropped
//   - a new index for a collection gets created
//   - a view gets dropped
//   - the properties of an existing view get modified
//   in the cluster, we need to track the changes of database objects which are
//   executed via other coordinators by tailing the changes in the AgencyCache,
//   and react accordingly. this is currently missing in this PR.
// - dropping caches when a database is dropped is not necessary however,
//   because each database has its own QueryPlanCache object
// - the cache implementation is intentionally kept simple. each per-database
//   cache object has a simple R/W lock, which may turn out to be a point of
//   mutex contention at some point. in this case, we could shard each cache
//   object so that it uses multiple buckets
// - we currently disable caching whenever one of the following query options
//   are used:
//   - forceOneShardAttributeValue
//   - inaccessibleCollections (Enterprise Edition only)
//   - restrictToShards ("shardIds" query option)
//   we need to figure out if there are additional query options that should
//   disable the use of the plan cache, or if we can lift the restrictions on
//   the above attributes. especially the "forceOneShardAttributeValue" is used
//   in reality, and we may want to support it.
// - we need to ensure that permissions for collections/views are still verified
//   even for queries that come from the plan cache. this is required because a
//   privileged user's query may insert the query into the plan cache, and an
//   unprivileged user's query may reuse the plan cache entry, even though the
//   unprivileged user should not have access to the collections/views used in
//   the query. we can copy the permissions checks from the query results cache
//   that we already have.
// - it is currently unclear if the query plan cache works as expected with the
//   following types of bind parameters:
//   - collection bind parameters: these must be part of the plan cache key.
//     the same query string used with different collection bind parameters must
//     lead to different plan cache keys and entries.
//   - attribute name bind parameters: these must be part of the plan cache key.
//     the same query string used with different attribute name bind parameters
//     must lead to different plan cache keys and entries. this is currently not
//     the case!
//   - value bind parameters: these should be fully ignored when constructing
//     the plan cache keys, i.e. different value bind parameter values should
//     lead to the same plan cache key if everything else is identical.
// - memory limit: currently, the plan cache has no memory limit nor memory
//   accounting. it is currently unclear if we want to put a limit on the cache,
//   and if it should be a global limit across all databases, or a per-database
//   plan cache limit.
// - memory accounting: we probably want memory accounting for the plan
// cache(s),
//   so that database admins can always check how much memory is actually in use
//   for cached plans.
// - the serialized query plans are relatively verbose right now. it may be
//   possible to reduce the verbosity of the cached plans somehow, by passing
//   other flags to the serialization function. this would reduce the memory
//   usage of cached plans, and would very likely speed up caching and creating
//   a plan from the plan cache.
// - observability is missing: when a query is created from a cached plan, there
//   is currently no visibility for this in the query result nor in the explain
//   output nor profile result. we should add a "cachedPlan" attribute in these
//   results which show the hash of the cached plan, if any. we should also
//   improve the explainer so that it displays the "cachedPlan" value.
// - when enabling the "optimizePlanForCaching" flag for a query, the generated
//   query execution plan may be worse than when not setting this flag. the
//   reason is that certain query optimizations are only possible when literal
//   values are used in the query, e.g. the "restrict-to-single-shard" optimizer
//   rule looks if there are filter conditions on the collections' shard key
//   attributes that include literal values. in this case, the optimizer is
//   sometimes able to figure out that the query will only touch a single shard,
//   and thus apply the "restrict-to-single-shard" optimizer rule. this
//   optimization is not possible when the filter condition on the shard key
//   attribute(s) uses a variable instead of a value literal.
//   it is currently unclear how to bring back this optimization in case a
//   cached plan is used. potentially it is an option to run a limited number
//   of optimizer rules on the plan that was retrieved from the cache.
// - using _value bind parameters_ in certain positions of the query string may
//   change the semantics of a query and thus may mandate the use of a different
//   query execution plan. it is currently unclear if this is the case or not,
//   but we should validate this. a few places for which this may be an issue:
//   - LIMIT @offset, @limit (if allowed by the parser): the offset and limit
//     values are part of the LimitNode, so using different offsets/limits
//     may require using different plans
//   - FOR ... OPTIONS @options (if allowed by the parser): OPTIONS can
//     influence the behavior of the query, so using different sets of options
//     may require using different plans
//   - INSERT/UPDATE/REPLACE/REMOVE/UPSERT ... OPTIONS @options (if allowed by
//     the parser): OPTIONS can influence the behavior of modification queries,
//     so using different sets of options may require using different plans
class QueryPlanCache {
 public:
  struct Key {
    QueryString queryString;

    // collection and attribute name bind parameters.
    // these parameters do not include value bind parameters
    std::shared_ptr<velocypack::Builder> bindParameters;
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
    std::vector<std::string> dataSources;

    // plan velocypack
    std::shared_ptr<velocypack::Builder> serializedPlan;
  };

  struct Entry {
    Key key;
    Value value;
  };

  QueryPlanCache(QueryPlanCache const&) = delete;
  QueryPlanCache& operator=(QueryPlanCache const&) = delete;

  QueryPlanCache();

  ~QueryPlanCache();

  std::shared_ptr<velocypack::Builder> lookup(Key const& key) const;

  void store(Key&& key, std::vector<std::string>&& dataSources,
             std::shared_ptr<velocypack::Builder> serializedPlan);

  Key createCacheKey(
      QueryString const& queryString,
      std::shared_ptr<velocypack::Builder> const& bindVars) const;

  void invalidate(std::string_view dataSource);

  void invalidateAll();

  static std::shared_ptr<velocypack::Builder> filterBindParameters(
      std::shared_ptr<velocypack::Builder> const& source);

 private:
  // mutex protecting _entries
  mutable std::shared_mutex _mutex;

  // mapping from plan cache key to stored plan velocypack
  std::unordered_map<Key, Value, KeyHasher, KeyEqual> _entries;
};

}  // namespace arangodb::aql
