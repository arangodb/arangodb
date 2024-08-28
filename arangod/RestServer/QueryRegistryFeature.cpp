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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include <algorithm>

#include "QueryRegistryFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/Query.h"
#include "Aql/QueryCache.h"
#include "Aql/QueryRegistry.h"
#include "Basics/GlobalResourceMonitor.h"
#include "Basics/NumberOfCores.h"
#include "Basics/PhysicalMemory.h"
#include "Basics/application-exit.h"
#include "Cluster/ServerState.h"
#include "FeaturePhases/ClusterFeaturePhase.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Metrics/CounterBuilder.h"
#include "Metrics/GaugeBuilder.h"
#include "Metrics/HistogramBuilder.h"
#include "Metrics/LogScale.h"
#include "Metrics/MetricsFeature.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

namespace {

uint64_t defaultMemoryLimit(uint64_t available, double reserveFraction,
                            double percentage) {
  if (available == 0) {
    // we don't know how much memory is available, so we cannot do any sensible
    // calculation
    return 0;
  }

  // this function will produce the following results for a reserveFraction of
  // 0.2 and a percentage of 0.75 for some common available memory values:
  //
  //    Available memory:            0      (0MiB)  Limit:            0
  //    unlimited, %mem:  n/a Available memory:    134217728    (128MiB)  Limit:
  //    33554432     (32MiB), %mem: 25.0 Available memory:    268435456 (256MiB)
  //    Limit:     67108864     (64MiB), %mem: 25.0 Available memory: 536870912
  //    (512MiB)  Limit:    201326592    (192MiB), %mem: 37.5 Available memory:
  //    805306368    (768MiB)  Limit:    402653184    (384MiB), %mem: 50.0
  //    Available memory:   1073741824   (1024MiB)  Limit:    603979776
  //    (576MiB), %mem: 56.2 Available memory:   2147483648   (2048MiB)  Limit:
  //    1288490189   (1228MiB), %mem: 60.0 Available memory:   4294967296
  //    (4096MiB)  Limit:   2576980377   (2457MiB), %mem: 60.0 Available memory:
  //    8589934592   (8192MiB)  Limit:   5153960755   (4915MiB), %mem: 60.0
  //    Available memory:  17179869184  (16384MiB)  Limit:  10307921511
  //    (9830MiB), %mem: 60.0 Available memory:  25769803776  (24576MiB)  Limit:
  //    15461882265  (14745MiB), %mem: 60.0 Available memory:  34359738368
  //    (32768MiB)  Limit:  20615843021  (19660MiB), %mem: 60.0 Available
  //    memory:  42949672960  (40960MiB)  Limit:  25769803776  (24576MiB),
  //    %mem: 60.0 Available memory:  68719476736  (65536MiB)  Limit:
  //    41231686041  (39321MiB), %mem: 60.0 Available memory: 103079215104
  //    (98304MiB)  Limit:  61847529063  (58982MiB), %mem: 60.0 Available
  //    memory: 137438953472 (131072MiB)  Limit:  82463372083  (78643MiB),
  //    %mem: 60.0 Available memory: 274877906944 (262144MiB)  Limit:
  //    164926744167 (157286MiB), %mem: 60.0 Available memory: 549755813888
  //    (524288MiB)  Limit: 329853488333 (314572MiB), %mem: 60.0

  // for a reserveFraction of 0.05 and a percentage of 0.95 it will produce:
  //
  //    Available memory:            0      (0MiB)  Limit:            0
  //    unlimited, %mem:  n/a Available memory:    134217728    (128MiB)  Limit:
  //    33554432     (32MiB), %mem: 25.0 Available memory:    268435456 (256MiB)
  //    Limit:     67108864     (64MiB), %mem: 25.0 Available memory: 536870912
  //    (512MiB)  Limit:    255013683    (243MiB), %mem: 47.5 Available memory:
  //    805306368    (768MiB)  Limit:    510027366    (486MiB), %mem: 63.3
  //    Available memory:   1073741824   (1024MiB)  Limit:    765041049
  //    (729MiB), %mem: 71.2 Available memory:   2147483648   (2048MiB)  Limit:
  //    1785095782   (1702MiB), %mem: 83.1 Available memory:   4294967296
  //    (4096MiB)  Limit:   3825205248   (3648MiB), %mem: 89.0 Available memory:
  //    8589934592   (8192MiB)  Limit:   7752415969   (7393MiB), %mem: 90.2
  //    Available memory:  17179869184  (16384MiB)  Limit:  15504831938
  //    (14786MiB), %mem: 90.2 Available memory:  25769803776  (24576MiB) Limit:
  //    23257247908  (22179MiB), %mem: 90.2 Available memory:  34359738368
  //    (32768MiB)  Limit:  31009663877  (29573MiB), %mem: 90.2 Available
  //    memory:  42949672960  (40960MiB)  Limit:  38762079846  (36966MiB),
  //    %mem: 90.2 Available memory:  68719476736  (65536MiB)  Limit:
  //    62019327755  (59146MiB), %mem: 90.2 Available memory: 103079215104
  //    (98304MiB)  Limit:  93028991631  (88719MiB), %mem: 90.2 Available
  //    memory: 137438953472 (131072MiB)  Limit: 124038655509 (118292MiB),
  //    %mem: 90.2 Available memory: 274877906944 (262144MiB)  Limit:
  //    248077311017 (236584MiB), %mem: 90.2 Available memory: 549755813888
  //    (524288MiB)  Limit: 496154622034 (473169MiB), %mem: 90.2

  // reserveFraction% of RAM will be considered as a reserve
  uint64_t reserve = static_cast<uint64_t>(available * reserveFraction);

  // minimum reserve memory is 256MB
  reserve = std::max<uint64_t>(reserve, static_cast<uint64_t>(256) << 20);

  TRI_ASSERT(available > 0);
  double f = double(1.0) - (double(reserve) / double(available));
  double dyn = (double(available) * f * percentage);
  if (dyn < 0.0) {
    dyn = 0.0;
  }

  return std::max(static_cast<uint64_t>(dyn),
                  static_cast<uint64_t>(0.25 * available));
}

}  // namespace

namespace arangodb {

std::atomic<aql::QueryRegistry*> QueryRegistryFeature::QUERY_REGISTRY{nullptr};

struct QueryTimeScale {
  static metrics::LogScale<double> scale() { return {2., 0.0, 50.0, 20}; }
};
struct SlowQueryTimeScale {
  static metrics::LogScale<double> scale() { return {2., 1.0, 2000.0, 10}; }
};

DECLARE_COUNTER(arangodb_aql_all_query_total,
                "Total number of AQL queries finished");
DECLARE_HISTOGRAM(arangodb_aql_query_time, QueryTimeScale,
                  "Execution time histogram for all AQL queries [s]");
DECLARE_HISTOGRAM(arangodb_aql_slow_query_time, SlowQueryTimeScale,
                  "Execution time histogram for slow AQL queries [s]");
DECLARE_COUNTER(arangodb_aql_total_query_time_msec_total,
                "Total execution time of all AQL queries [ms]");
DECLARE_GAUGE(arangodb_aql_current_query, uint64_t,
              "Current number of AQL queries executing");
DECLARE_GAUGE(
    arangodb_aql_global_memory_usage, uint64_t,
    "Total memory usage of all AQL queries executing [bytes], granularity: " +
        std::to_string(ResourceMonitor::chunkSize) + " bytes steps");
DECLARE_GAUGE(arangodb_aql_global_memory_limit, uint64_t,
              "Total memory limit for all AQL queries combined [bytes]");
DECLARE_COUNTER(arangodb_aql_global_query_memory_limit_reached_total,
                "Number of global AQL query memory limit violations");
DECLARE_COUNTER(arangodb_aql_local_query_memory_limit_reached_total,
                "Number of local AQL query memory limit violations");
DECLARE_GAUGE(arangodb_aql_cursors_active, uint64_t,
              "Total amount of active AQL query results cursors");
DECLARE_GAUGE(arangodb_aql_cursors_memory_usage, uint64_t,
              "Total memory usage of active query result cursors");

QueryRegistryFeature::QueryRegistryFeature(Server& server,
                                           metrics::MetricsFeature& metrics)
    : ArangodFeature{server, *this},
      _trackingEnabled(true),
      _trackSlowQueries(true),
      _trackQueryString(true),
      _trackBindVars(true),
      _trackDataSources(false),
      _failOnWarning(aql::QueryOptions::defaultFailOnWarning),
      _requireWith(false),
      _queryCacheIncludeSystem(false),
      _queryMemoryLimitOverride(true),
#ifdef USE_ENTERPRISE
      _smartJoins(true),
      _parallelizeTraversals(true),
#endif
      _allowCollectionsInExpressions(false),
      _logFailedQueries(false),
      _maxAsyncPrefetchSlotsTotal(256),
      _maxAsyncPrefetchSlotsPerQuery(32),
      _maxQueryStringLength(4096),
      _maxCollectionsPerQuery(2048),
      _peakMemoryUsageThreshold(1073741824),  // 1GB
      _queryGlobalMemoryLimit(
          defaultMemoryLimit(PhysicalMemory::getValue(), 0.1, 0.90)),
      _queryMemoryLimit(
          defaultMemoryLimit(PhysicalMemory::getValue(), 0.2, 0.75)),
      _maxDNFConditionMembers(aql::QueryOptions::defaultMaxDNFConditionMembers),
      _queryMaxRuntime(aql::QueryOptions::defaultMaxRuntime),
      _maxQueryPlans(aql::QueryOptions::defaultMaxNumberOfPlans),
      _maxNodesPerCallstack(aql::QueryOptions::defaultMaxNodesPerCallstack),
      _queryCacheMaxResultsCount(0),
      _queryCacheMaxResultsSize(0),
      _queryCacheMaxEntrySize(0),
      _maxParallelism(4),
      _slowQueryThreshold(10.0),
      _slowStreamingQueryThreshold(10.0),
      _queryRegistryTTL(0.0),
      _queryCacheMode("off"),
      _queryTimes(metrics.add(arangodb_aql_query_time{})),
      _slowQueryTimes(metrics.add(arangodb_aql_slow_query_time{})),
      _totalQueryExecutionTime(
          metrics.add(arangodb_aql_total_query_time_msec_total{})),
      _queriesCounter(metrics.add(arangodb_aql_all_query_total{})),
      _runningQueries(metrics.add(arangodb_aql_current_query{})),
      _globalQueryMemoryUsage(metrics.add(arangodb_aql_global_memory_usage{})),
      _globalQueryMemoryLimit(metrics.add(arangodb_aql_global_memory_limit{})),
      _globalQueryMemoryLimitReached(
          metrics.add(arangodb_aql_global_query_memory_limit_reached_total{})),
      _localQueryMemoryLimitReached(
          metrics.add(arangodb_aql_local_query_memory_limit_reached_total{})),
      _activeCursors(metrics.add(arangodb_aql_cursors_active{})),
      _cursorsMemoryUsage(metrics.add(arangodb_aql_cursors_memory_usage{})) {
  static_assert(
      Server::isCreatedAfter<QueryRegistryFeature, metrics::MetricsFeature>());

  setOptional(false);
#ifdef USE_V8
  startsAfter<V8FeaturePhase>();
#else
  startsAfter<application_features::ClusterFeaturePhase>();
#endif

  auto properties = arangodb::aql::QueryCache::instance()->properties();
  _queryCacheMaxResultsCount = properties.maxResultsCount;
  _queryCacheMaxResultsSize = properties.maxResultsSize;
  _queryCacheMaxEntrySize = properties.maxEntrySize;
  _queryCacheIncludeSystem = properties.includeSystem;
}

QueryRegistryFeature::~QueryRegistryFeature() = default;

void QueryRegistryFeature::collectOptions(
    std::shared_ptr<ProgramOptions> options) {
  options->addSection("query", "AQL queries");

  options->addOldOption("database.query-cache-mode", "query.cache-mode");
  options->addOldOption("database.query-cache-max-results",
                        "query.cache-entries");
  options->addOldOption("database.disable-query-tracking", "query.tracking");

  // option obsoleted since 3.12.1, because the APIs are turned on by default.
  options->addObsoleteOption("query.enable-debug-apis",
                             "Whether to enable query debug APIs.", false);

  options
      ->addOption("--query.global-memory-limit",
                  "The memory threshold for all AQL queries combined "
                  "(in bytes, 0 = no limit).",
                  new UInt64Parameter(&_queryGlobalMemoryLimit,
                                      PhysicalMemory::getValue()),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Dynamic))
      .setIntroducedIn(30800)
      .setLongDescription(R"(You can use this option to set a limit on the
combined estimated memory usage of all AQL queries (in bytes). If this option
has a value of `0`, then no global memory limit is in place. This is also the
default value and the same behavior as in version 3.7 and older.

If you set this option to a value greater than zero, then the total memory usage
of all AQL queries is limited approximately to the configured value. The limit
is enforced by each server node in a cluster independently, i.e. it can be set
separately for Coordinators, DB-Servers etc. The memory usage of a query that
runs on multiple servers in parallel is not summed up, but tracked separately on
each server.

If a memory allocation in a query would lead to the violation of the configured
global memory limit, then the query is aborted with error code 32
("resource limit exceeded").

The global memory limit is approximate, in the same fashion as the per-query
memory limit exposed by the option `--query.memory-limit` is. The global memory
tracking has a granularity of 32 KiB chunks.

If both, `--query.global-memory-limit` and `--query.memory-limit`, are set, you
must set the former at least as high as the latter.)");

  options
      ->addOption(
          "--query.memory-limit",
          "The memory threshold per AQL query (in bytes, 0 = no limit).",
          new UInt64Parameter(&_queryMemoryLimit, PhysicalMemory::getValue()),
          arangodb::options::makeDefaultFlags(
              arangodb::options::Flags::Dynamic))
      .setLongDescription(R"(The default maximum amount of memory (in bytes)
that a single AQL query can use. When a single AQL query reaches the specified
limit value, the query is aborted with a *resource limit exceeded* exception.
In a cluster, the memory accounting is done per server, so the limit value is
effectively a memory limit per query per server node.

Some operations, namely calls to AQL functions and their intermediate results, 
are not properly tracked.

You can override the limit by setting the `memoryLimit` option for individual
queries when running them. Overriding the per-query limit value is only possible
if the `--query.memory-limit-override` option is set to `true`.

The default per-query memory limit value in version 3.8 and later depends on
the amount of available RAM. In version 3.7 and older, the default value was
`0`, meaning "unlimited".

The default values are:

```
Available memory:            0      (0MiB)  Limit:            0   unlimited, %mem:  n/a
Available memory:    134217728    (128MiB)  Limit:     33554432     (32MiB), %mem: 25.0
Available memory:    268435456    (256MiB)  Limit:     67108864     (64MiB), %mem: 25.0
Available memory:    536870912    (512MiB)  Limit:    201326592    (192MiB), %mem: 37.5
Available memory:    805306368    (768MiB)  Limit:    402653184    (384MiB), %mem: 50.0
Available memory:   1073741824   (1024MiB)  Limit:    603979776    (576MiB), %mem: 56.2
Available memory:   2147483648   (2048MiB)  Limit:   1288490189   (1228MiB), %mem: 60.0
Available memory:   4294967296   (4096MiB)  Limit:   2576980377   (2457MiB), %mem: 60.0
Available memory:   8589934592   (8192MiB)  Limit:   5153960755   (4915MiB), %mem: 60.0
Available memory:  17179869184  (16384MiB)  Limit:  10307921511   (9830MiB), %mem: 60.0
Available memory:  25769803776  (24576MiB)  Limit:  15461882265  (14745MiB), %mem: 60.0
Available memory:  34359738368  (32768MiB)  Limit:  20615843021  (19660MiB), %mem: 60.0
Available memory:  42949672960  (40960MiB)  Limit:  25769803776  (24576MiB), %mem: 60.0
Available memory:  68719476736  (65536MiB)  Limit:  41231686041  (39321MiB), %mem: 60.0
Available memory: 103079215104  (98304MiB)  Limit:  61847529063  (58982MiB), %mem: 60.0
Available memory: 137438953472 (131072MiB)  Limit:  82463372083  (78643MiB), %mem: 60.0
Available memory: 274877906944 (262144MiB)  Limit: 164926744167 (157286MiB), %mem: 60.0
Available memory: 549755813888 (524288MiB)  Limit: 329853488333 (314572MiB), %mem: 60.0
```

You can set a global memory limit for the total memory used by all AQL queries
that currently execute via the `--query.global-memory-limit` option.

From ArangoDB 3.8 on, the per-query memory tracking has a granularity of 32 KB 
chunks. That means checking for memory limits such as "1" (e.g. for testing) 
may not make a query fail if the total memory allocations in the query don't 
exceed 32 KiB. The effective lowest memory limit value that can be enforced is
thus 32 KiB. Memory limit values higher than 32 KiB will be checked whenever the
total memory allocations cross a 32 KiB boundary.)");

  options
      ->addOption("--query.memory-limit-override",
                  "Allow increasing the per-query memory limits for individual "
                  "queries.",
                  new BooleanParameter(&_queryMemoryLimitOverride))
      .setIntroducedIn(30800)
      .setLongDescription(R"(You can use this option to control whether
individual AQL queries can increase their memory limit via the `memoryLimit`
query option. This is the default, so a query that increases its memory limit is
allowed to use more memory than set via the `--query.memory-limit` startup
option value.

If the option is set to `false`, individual queries can only lower their maximum
allowed memory usage but not increase it.)");

  options
      ->addOption(
          "--query.max-runtime",
          "The runtime threshold for AQL queries (in seconds, 0 = no limit).",
          new DoubleParameter(&_queryMaxRuntime, /*base*/ 1.0,
                              /*minValue*/ 0.0))
      .setLongDescription(R"(Sets a default maximum runtime for AQL queries.

The default value is `0`, meaning that the runtime of AQL queries is not
limited. If you set it to any positive value, it restricts the runtime of all
AQL queries, unless you override it with the `maxRuntime` query option on a
per-query basis.

If a query exceeds the configured runtime, it is killed on the next occasion
when the query checks its own status. Killing is best effort-based, so it is not
guaranteed that a query will no longer than exactly the configured amount of
time.

**Warning**: This option affects all queries in all databases, including queries
issued for administration and database-internal purposes.)");

  options->addOption("--query.tracking", "Whether to track queries.",
                     new BooleanParameter(&_trackingEnabled));

  options->addOption("--query.tracking-slow-queries",
                     "Whether to track slow queries.",
                     new BooleanParameter(&_trackSlowQueries));

  options->addOption("--query.tracking-with-querystring",
                     "Whether to track the query string.",
                     new BooleanParameter(&_trackQueryString));

  options
      ->addOption("--query.tracking-with-bindvars",
                  "Whether to track bind variable of AQL queries.",
                  new BooleanParameter(&_trackBindVars))
      .setLongDescription(R"(If set to `true`, then the bind variables are
tracked and shown for all running and slow AQL queries. This also enables the
display of bind variable values in the list of cached AQL query results. This
option only has an effect if `--query.tracking` is set to `true` or if the query
results cache is used.

You can disable tracking and displaying bind variable values by setting the
option to `false`.)");

  options->addOption("--query.tracking-with-datasources",
                     "Whether to track data sources of AQL queries.",
                     new BooleanParameter(&_trackDataSources));

  options
      ->addOption("--query.fail-on-warning",
                  "Whether AQL queries should fail with errors even for "
                  "recoverable warnings.",
                  new BooleanParameter(&_failOnWarning))
      .setLongDescription(R"(If set to `true`, AQL queries that produce
warnings are instantly aborted and throw an exception. This option can be set
to catch obvious issues with AQL queries early.

If set to `false`, AQL queries that produce warnings are not aborted and return
the warnings along with the query results.

You can override the option for each individual AQL query via the
`failOnWarning` attribute.)");

  options
      ->addOption("--query.require-with",
                  "Whether AQL queries should require the "
                  "`WITH collection-name` clause even on single servers "
                  "(enable this to remove this behavior difference between "
                  "single server and cluster).",
                  new BooleanParameter(&_requireWith))
      .setIntroducedIn(30711)
      .setIntroducedIn(30800)
      .setLongDescription(R"(If set to `true`, AQL queries in single server
mode also require `WITH` clauses in AQL queries where a cluster installation
would require them.

The option is set to `false` by default, but you can turn it on in single
servers to remove this behavior difference between single servers and clusters,
making a later transition from single server to cluster easier.)");

  options
      ->addOption("--query.slow-threshold",
                  "The threshold for slow AQL queries (in seconds).",
                  new DoubleParameter(&_slowQueryThreshold))
      .setLongDescription(R"(You can control after what execution time an AQL
query is considered "slow" with this option. Any slow queries that exceed the
specified execution time are logged when they are finished.

You can turn off the tracking of slow queries entirely by setting the option
`--query.tracking` to `false`.)");

  options
      ->addOption("--query.slow-streaming-threshold",
                  "The threshold for slow streaming AQL queries "
                  "(in seconds).",
                  new DoubleParameter(&_slowStreamingQueryThreshold))
      .setLongDescription(R"(You can control after what execution time
streaming AQL queries are considered "slow" with this option. It exists to give
streaming queries a separate, potentially higher timeout value than for regular
queries. Streaming queries are often executed in lockstep with application data
processing logic, which then also accounts for the queries' runtime. It is thus
expected that the lifetime of streaming queries is longer than for regular
queries.)");

  options
      ->addOption("--query.cache-mode",
                  "The mode for the AQL query result cache. Can be \"on\", "
                  "\"off\", or \"demand\".",
                  new StringParameter(&_queryCacheMode))
      .setLongDescription(R"(Toggles the AQL query results cache behavior.
The possible values are:

- `off`: do not use query results cache
- `on`: always use query results cache, except for queries that have their
  `cache` attribute set to `false`
- `demand`: use query results cache only for queries that have their `cache`
  attribute set to `true`)");

  options
      ->addOption(
          "--query.cache-entries",
          "The maximum number of results in query result cache per database.",
          new UInt64Parameter(&_queryCacheMaxResultsCount))
      .setLongDescription(R"(If a query is eligible for caching and the number
of items in the database's query cache is equal to this threshold value, another
cached query result is removed from the cache.

This option only has an effect if the query cache mode is set to either `on` or
`demand`.)");

  options
      ->addOption("--query.cache-entries-max-size",
                  "The maximum cumulated size of results in the query result "
                  "cache per database (in bytes).",
                  new UInt64Parameter(&_queryCacheMaxResultsSize))
      .setLongDescription(R"(When a query result is inserted into the query
results cache, it is checked if the total size of cached results would exceed
this value, and if so, another cached query result is removed from the cache
before a new one is inserted.

This option only has an effect if the query cache mode is set to either `on` or
`demand`.)");

  options
      ->addOption("--query.cache-entry-max-size",
                  "The maximum size of an individual result entry in query "
                  "result cache (in bytes).",
                  new UInt64Parameter(&_queryCacheMaxEntrySize))
      .setLongDescription(R"(Query results are only eligible for caching if
their size does not exceed this setting's value.)");

  options
      ->addOption("--query.cache-include-system-collections",
                  "Whether to include system collection queries in "
                  "the query result cache.",
                  new BooleanParameter(&_queryCacheIncludeSystem))
      .setLongDescription(R"(Not storing these results is normally beneficial
if you use the query results cache, as queries on system collections are
internal to ArangoDB and use space in the query results cache unnecessarily.)");

  options
      ->addOption(
          "--query.optimizer-max-plans",
          "The maximum number of query plans to create for a query.",
          new UInt64Parameter(&_maxQueryPlans, /*base*/ 1, /*minValue*/ 1))
      .setLongDescription(R"(You can control how many different query execution
plans the AQL query optimizer generates at most for any given AQL query with
this option. Normally, the AQL query optimizer generates a single execution plan
per AQL query, but there are some cases in which it creates multiple competing
plans.

More plans can lead to better optimized queries. However, plan creation has its
costs. The more plans are created and shipped through the optimization pipeline,
the more time is spent in the optimizer. You can lower the number to make the
optimizer stop creating additional plans when it has already created enough
plans.

Note that this setting controls the default maximum number of plans to create.
The value can still be adjusted on a per-query basis by setting the
`maxNumberOfPlans` attribute for individual queries.)");

  options
      ->addOption("--query.max-nodes-per-callstack",
                  "The maximum number of execution nodes on the callstack "
                  "before splitting the remaining nodes into a separate thread",
                  new UInt64Parameter(&_maxNodesPerCallstack),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Uncommon))
      .setIntroducedIn(30900);

  options->addOption(
      "--query.registry-ttl",
      "The default time-to-live of cursors and query snippets (in seconds). "
      "If set to 0 or lower, the value defaults to 30 for single server "
      "instances and 600 for Coordinator instances.",
      new DoubleParameter(&_queryRegistryTTL),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));

#ifdef USE_ENTERPRISE
  options->addOption("--query.smart-joins",
                     "Whether to enable the SmartJoins query optimization.",
                     new BooleanParameter(&_smartJoins),
                     arangodb::options::makeDefaultFlags(
                         arangodb::options::Flags::Uncommon,
                         arangodb::options::Flags::Enterprise));

  options->addOption("--query.parallelize-traversals",
                     "Whether to enable traversal parallelization.",
                     new BooleanParameter(&_parallelizeTraversals),
                     arangodb::options::makeDefaultFlags(
                         arangodb::options::Flags::Uncommon,
                         arangodb::options::Flags::Enterprise));

  // this is an Enterprise-only option
  // in Community Edition, _maxParallelism will stay at its default value
  // (currently 4), but will not be used.
  options->addOption(
      "--query.max-parallelism",
      "The maximum number of threads to use for a single query; the "
      "actual query execution may use less depending on various factors.",
      new UInt64Parameter(&_maxParallelism),
      arangodb::options::makeDefaultFlags(
          arangodb::options::Flags::Uncommon,
          arangodb::options::Flags::Enterprise));
#endif

  options
      ->addOption("--query.allow-collections-in-expressions",
                  "Allow full collections to be used in AQL expressions.",
                  new BooleanParameter(&_allowCollectionsInExpressions),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnCoordinator,
                      arangodb::options::Flags::OnSingle,
                      arangodb::options::Flags::Uncommon))
      .setIntroducedIn(30800)
      .setDeprecatedIn(30900)
      .setLongDescription(R"(If set to `true`, using collection names in
arbitrary places in AQL expressions is allowed, although using collection names
like this is very likely unintended.

For example, consider the following query:

```aql
FOR doc IN collection RETURN collection
```

Here, the collection name is `collection`, and its usage in the `FOR` loop is
intended and valid. However, `collection` is also used in the `RETURN`
statement, which is legal but potentially unintended. It should likely be
`RETURN doc` or `RETURN doc.someAttribute` instead. Otherwise, the entire
collection is materialized and returned as many times as there are documents in
the collection. This can take a long time and even lead to out-of-memory crashes
in the worst case.

If you set the option to `false`, such unintentional usage of collection names
in queries is prohibited, and instead makes the query fail with error 1568
("collection used as expression operand").

The default value of the option was `true` in v3.8, meaning that potentially
unintended usage of collection names in queries were still allowed. In v3.9,
the default value changes to `false`. The option is also deprecated from
3.9.0 on and will be removed in future versions. From then on, unintended
usage of collection names will always be disallowed.)");

  options
      ->addOption("--query.max-artifact-log-length",
                  "The maximum length of query strings and bind parameter "
                  "values in logs before they get truncated.",
                  new SizeTParameter(&_maxQueryStringLength),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnCoordinator,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(30905)
      .setIntroducedIn(31002)
      .setLongDescription(R"(This option allows you to truncate overly long
query strings and bind parameter values to a reasonable length in log files.)");

  options
      ->addOption("--query.log-memory-usage-threshold",
                  "Log queries that have a peak memory usage larger than this "
                  "threshold.",
                  new UInt64Parameter(&_peakMemoryUsageThreshold),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnCoordinator,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(30905)
      .setIntroducedIn(31002)
      .setLongDescription(R"(A warning is logged if queries exceed the
specified threshold. This is useful for finding queries that use a large
amount of memory.)");

  options
      ->addOption("--query.log-failed", "Whether to log failed AQL queries.",
                  new BooleanParameter(&_logFailedQueries),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnCoordinator,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(30905)
      .setIntroducedIn(31002)
      .setLongDescription(R"(If set to `true`, all failed AQL queries are
logged to the server log. You can use this option during development, or to
catch unexpected failed queries in production.)");

  options
      ->addOption(
          "--query.max-dnf-condition-members",
          "The maximum number of OR sub-nodes in the internal representation "
          "of an AQL FILTER condition.",
          new SizeTParameter(&_maxDNFConditionMembers),
          arangodb::options::makeFlags(
              arangodb::options::Flags::Uncommon,
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnCoordinator,
              arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31100)
      .setLongDescription(R"(Yon can use this option to limit the computation
time and memory usage when converting complex AQL FILTER conditions into the
internal DNF (disjunctive normal form) format. FILTER conditions with a lot of
logical branches (AND, OR, NOT) can take a large amount of processing time and
memory. This startup option limits the computation time and memory usage for
such conditions.

Once the threshold value is reached during the DNF conversion of a FILTER
condition, the conversion is aborted, and the query continues with a simplified
internal representation of the condition, which cannot be used for index
lookups.)");

  options
      ->addOption(
          "--query.max-collections-per-query",
          "The maximum number of collections/shards that can be used in "
          "one AQL query.",
          new SizeTParameter(&_maxCollectionsPerQuery),
          arangodb::options::makeDefaultFlags(
              arangodb::options::Flags::Uncommon,
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnCoordinator,
              arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31007);

  options
      ->addOption(
          "--query.max-total-async-prefetch-slots",
          "The maximum total number of slots available for asynchronous "
          "prefetching across all AQL queries.",
          new SizeTParameter(&_maxAsyncPrefetchSlotsTotal),
          arangodb::options::makeFlags(
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnCoordinator,
              arangodb::options::Flags::OnDBServer,
              arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31200);

  options
      ->addOption(
          "--query.max-query-async-prefetch-slots",
          "The maximum per-query number of slots available for asynchronous "
          "prefetching inside any AQL query.",
          new SizeTParameter(&_maxAsyncPrefetchSlotsPerQuery),
          arangodb::options::makeFlags(
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnCoordinator,
              arangodb::options::Flags::OnDBServer,
              arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31200);
}

void QueryRegistryFeature::validateOptions(
    std::shared_ptr<ProgramOptions> options) {
  if (_queryGlobalMemoryLimit > 0 &&
      _queryMemoryLimit > _queryGlobalMemoryLimit) {
    LOG_TOPIC("2af5f", FATAL, Logger::AQL)
        << "invalid value for `--query.global-memory-limit`. expecting 0 or a "
           "value >= `--query.memory-limit`";
    FATAL_ERROR_EXIT();
  }

  if (_maxAsyncPrefetchSlotsPerQuery > _maxAsyncPrefetchSlotsTotal) {
    LOG_TOPIC("84882", FATAL, Logger::AQL)
        << "invalid values for `--query.max-total-async-prefetch-slots` ("
        << _maxAsyncPrefetchSlotsTotal
        << ") / `--query.max-query-async-prefetch-slots` ("
        << _maxAsyncPrefetchSlotsPerQuery
        << "). the latter option must not be set to a higher value than the "
           "former";
    FATAL_ERROR_EXIT();
  }

  // cap the value somehow. creating this many plans really does not make sense
  _maxQueryPlans = std::min(_maxQueryPlans, decltype(_maxQueryPlans)(1024));

  _maxParallelism =
      std::clamp(_maxParallelism, static_cast<uint64_t>(1),
                 static_cast<uint64_t>(NumberOfCores::getValue()));

  if (_queryRegistryTTL <= 0) {
    TRI_ASSERT(ServerState::instance()->getRole() !=
               ServerState::ROLE_UNDEFINED);
    // set to default value based on instance type
    _queryRegistryTTL =
        ServerState::instance()->isSingleServer() ? 30.0 : 600.0;
  }

  TRI_ASSERT(_queryGlobalMemoryLimit == 0 ||
             _queryMemoryLimit <= _queryGlobalMemoryLimit);

  aql::QueryOptions::defaultMemoryLimit = _queryMemoryLimit;
  aql::QueryOptions::defaultMaxNumberOfPlans = _maxQueryPlans;
  aql::QueryOptions::defaultMaxNodesPerCallstack = _maxNodesPerCallstack;
  aql::QueryOptions::defaultMaxDNFConditionMembers = _maxDNFConditionMembers;
  aql::QueryOptions::defaultMaxRuntime = _queryMaxRuntime;
  aql::QueryOptions::defaultTtl = _queryRegistryTTL;
  aql::QueryOptions::defaultFailOnWarning = _failOnWarning;
  aql::QueryOptions::allowMemoryLimitOverride = _queryMemoryLimitOverride;
}

void QueryRegistryFeature::prepare() {
  // set the global memory limit
  GlobalResourceMonitor::instance().memoryLimit(_queryGlobalMemoryLimit);
  // prepare gauge value
  _globalQueryMemoryLimit = _queryGlobalMemoryLimit;

#ifndef ARANGODB_USE_GOOGLE_TESTS
  // we are now intentionally not printing this message during testing,
  // because otherwise it would be printed a *lot* of times
  // note that options() can be a nullptr during unit testing
  if (server().options() != nullptr &&
      !server().options()->processingResult().touched("--query.memory-limit")) {
    LOG_TOPIC("f6e0e", INFO, Logger::AQL)
        << "memory limit per AQL query automatically set to "
        << _queryMemoryLimit << " bytes. "
        << "to modify this value, please adjust the startup option "
           "`--query.memory-limit`";
  }
#endif

  if (ServerState::instance()->isCoordinator()) {
    // turn the query cache off on the coordinator, as it is not implemented
    // for the cluster
    _queryCacheMode = "off";
  }

  // configure the query cache
  arangodb::aql::QueryCacheProperties properties{
      arangodb::aql::QueryCache::modeString(_queryCacheMode),
      _queryCacheMaxResultsCount,
      _queryCacheMaxResultsSize,
      _queryCacheMaxEntrySize,
      _queryCacheIncludeSystem,
      _trackBindVars};
  arangodb::aql::QueryCache::instance()->properties(properties);
  // create the query registry
  _queryRegistry = std::make_unique<aql::QueryRegistry>(_queryRegistryTTL);
  QUERY_REGISTRY.store(_queryRegistry.get(), std::memory_order_release);

  _asyncPrefetchSlotsManager.configure(_maxAsyncPrefetchSlotsTotal,
                                       _maxAsyncPrefetchSlotsPerQuery);
}

void QueryRegistryFeature::beginShutdown() {
  TRI_ASSERT(_queryRegistry != nullptr);
  _queryRegistry->disallowInserts();
}

void QueryRegistryFeature::stop() {
  TRI_ASSERT(_queryRegistry != nullptr);
  _queryRegistry->disallowInserts();
  _queryRegistry->destroyAll();
}

void QueryRegistryFeature::unprepare() {
  // clear the query registry
  QUERY_REGISTRY.store(nullptr, std::memory_order_release);
}

void QueryRegistryFeature::updateMetrics() {
  GlobalResourceMonitor const& global = GlobalResourceMonitor::instance();
  _globalQueryMemoryUsage = global.current();
  _globalQueryMemoryLimit = global.memoryLimit();

  auto stats = global.stats();
  _globalQueryMemoryLimitReached = stats.globalLimitReached;
  _localQueryMemoryLimitReached = stats.localLimitReached;
}

void QueryRegistryFeature::trackQueryStart() noexcept { ++_runningQueries; }

void QueryRegistryFeature::trackQueryEnd(double time) {
  ++_queriesCounter;
  _queryTimes.count(time);
  _totalQueryExecutionTime += static_cast<uint64_t>(1000.0 * time);
  --_runningQueries;
}

void QueryRegistryFeature::trackSlowQuery(double time) {
  // query is already counted here as normal query, so don't count it
  // again in _queryTimes or _totalQueryExecutionTime
  _slowQueryTimes.count(time);
}

aql::AsyncPrefetchSlotsManager&
QueryRegistryFeature::asyncPrefetchSlotsManager() noexcept {
  return _asyncPrefetchSlotsManager;
}

}  // namespace arangodb
