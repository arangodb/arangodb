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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////
#include "Metrics/MetricsFeature.h"

#include <frozen/unordered_set.h>
#include <velocypack/Builder.h>

#include <chrono>

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/GreetingsFeaturePhase.h"
#include "Agency/Node.h"
#include "Basics/application-exit.h"
#include "Basics/debugging.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ServerState.h"
#include "Containers/FlatHashSet.h"
#include "Logger/LoggerFeature.h"
#include "Metrics/ClusterMetricsFeature.h"
#include "Metrics/Metric.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "Statistics/StatisticsFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"

namespace arangodb::metrics {

template<typename Server>
MetricsFeature::MetricsFeature(
    Server& server,
    LazyApplicationFeatureReference<QueryRegistryFeature>
        lazyQueryRegistryFeatureRef,
    LazyApplicationFeatureReference<StatisticsFeature> lazyStatisticsFeatureRef,
    LazyApplicationFeatureReference<EngineSelectorFeature>
        lazyEngineSelectorFeatureRef,
    LazyApplicationFeatureReference<ClusterMetricsFeature>
        lazyClusterMetricsFeatureRef,
    LazyApplicationFeatureReference<ClusterFeature> lazyClusterFeatureRef)
    : ApplicationFeature{server, *this},
      _lazyQueryRegistryFeatureRef(std::move(lazyQueryRegistryFeatureRef)),
      _lazyStatisticsFeatureRef(std::move(lazyStatisticsFeatureRef)),
      _lazyEngineSelectorFeatureRef(std::move(lazyEngineSelectorFeatureRef)),
      _lazyClusterMetricsFeatureRef(std::move(lazyClusterMetricsFeatureRef)),
      _lazyClusterFeatureRef(std::move(lazyClusterFeatureRef)),
      _export{true},
      _exportReadWriteMetrics{false},
      _ensureWhitespace{true},
      _usageTrackingModeString{"disabled"},
      _usageTrackingMode{UsageTrackingMode::kDisabled} {
  setOptional(false);
  startsAfter<LoggerFeature, Server>();
  startsBefore<application_features::GreetingsFeaturePhase, Server>();
}

template MetricsFeature::MetricsFeature(
    ArangodServer&, LazyApplicationFeatureReference<QueryRegistryFeature>,
    LazyApplicationFeatureReference<StatisticsFeature>,
    LazyApplicationFeatureReference<EngineSelectorFeature>,
    LazyApplicationFeatureReference<ClusterMetricsFeature>,
    LazyApplicationFeatureReference<ClusterFeature>);

void MetricsFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  _serverStatistics =
      std::make_unique<ServerStatistics>(*this, StatisticsFeature::time());

  options->addOption(
      "--server.export-metrics-api", "Whether to enable the metrics API.",
      new options::BooleanParameter(&_export),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));

  options
      ->addOption("--server.export-read-write-metrics",
                  "Whether to enable metrics for document reads and writes.",
                  new options::BooleanParameter(&_exportReadWriteMetrics),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Uncommon))
      .setLongDescription(R"(Enabling this option exposes the following
additional metrics via the `GET /_admin/metrics/v2` endpoint:

- `arangodb_document_writes_total`
- `arangodb_document_writes_replication_total`
- `arangodb_document_insert_time`
- `arangodb_document_read_time`
- `arangodb_document_update_time`
- `arangodb_document_replace_time`
- `arangodb_document_remove_time`
- `arangodb_collection_truncates_total`
- `arangodb_collection_truncates_replication_total`
- `arangodb_collection_truncate_time`
)");

  options
      ->addOption(
          "--server.ensure-whitespace-metrics-format",
          "Set to `true` to ensure whitespace between the exported metric "
          "value and the preceding token (metric name or labels) in the "
          "metrics output.",
          new options::BooleanParameter(&_ensureWhitespace),
          arangodb::options::makeDefaultFlags(
              arangodb::options::Flags::Uncommon))
      .setIntroducedIn(31006)
      .setLongDescription(R"(Using the whitespace characters in the output may
be required to make the metrics output compatible with some processing tools,
although Prometheus itself doesn't need it.)");

  std::unordered_set<std::string> modes = {"disabled", "enabled-per-shard",
                                           "enabled-per-shard-per-user"};
  options
      ->addOption(
          "--server.export-shard-usage-metrics",
          "Whether or not to export shard usage metrics.",
          new options::DiscreteValuesParameter<options::StringParameter>(
              &_usageTrackingModeString, modes),
          arangodb::options::makeFlags(
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnDBServer))
      .setIntroducedIn(31200)
      .setLongDescription(R"(This option can be used to make DB-Servers export
detailed shard usage metrics.

- By default, this option is set to `disabled` so that no shard usage metrics
  are exported.

- Set the option to `enabled-per-shard` to make DB-Servers collect per-shard
  usage metrics whenever a shard is accessed.

- Set this option to `enabled-per-shard-per-user` to make DB-Servers collect
  usage metrics per shard and per user whenever a shard is accessed.

Note that enabling shard usage metrics can produce a lot of metrics if there 
are many shards and/or users in the system.)");
}

std::shared_ptr<Metric> MetricsFeature::doAdd(Builder& builder) {
  auto metric = builder.build();
  TRI_ASSERT(metric != nullptr);
  MetricKeyView key{metric->name(), metric->labels()};
  std::lock_guard lock{_mutex};
  auto [it, inserted] = _registry.try_emplace(key, metric);
  if (!inserted) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        absl::StrCat(builder.type(), " ", metric->name(), ":", metric->labels(),
                     " already exists"));
  }
  return (*it).second;
}

std::shared_ptr<Metric> MetricsFeature::doEnsureMetric(Builder& builder) {
  auto metric = builder.build();
  TRI_ASSERT(metric != nullptr);
  MetricKeyView key{metric->name(), metric->labels()};
  {
    // happy path: check if metric already exists and if so, return it
    std::shared_lock lock{_mutex};
    if (auto it = _registry.find(key); it != _registry.end()) {
      return (*it).second;
    }
  }
  // slow path: create new metric under exclusive lock
  std::lock_guard lock{_mutex};
  // insertion can fail here because someone else concurrently inserted the
  // metric. this is fine, because in that case we simply return that
  // version.
  auto [it, inserted] = _registry.try_emplace(key, metric);
  return (*it).second;
}

std::shared_ptr<Metric> MetricsFeature::doAddDynamic(Builder& builder) {
  auto metric = doEnsureMetric(builder);
  metric->setDynamic();
  return metric;
}

Metric* MetricsFeature::get(MetricKeyView const& key) const {
  std::shared_lock lock{_mutex};
  auto it = _registry.find(key);
  if (it == _registry.end()) {
    return nullptr;
  }
  return it->second.get();
}

bool MetricsFeature::remove(Builder const& builder) {
  MetricKeyView key{builder.name(), builder.labels()};
  std::lock_guard guard{_mutex};
  return _registry.erase(key) != 0;
}

bool MetricsFeature::exportAPI() const noexcept { return _export; }

bool MetricsFeature::ensureWhitespace() const noexcept {
  return _ensureWhitespace;
}

MetricsFeature::UsageTrackingMode MetricsFeature::usageTrackingMode()
    const noexcept {
  return _usageTrackingMode;
}

void MetricsFeature::validateOptions(std::shared_ptr<options::ProgramOptions>) {
  if (_exportReadWriteMetrics) {
    serverStatistics().setupDocumentMetrics();
  }

  // translate usage tracking mode string to enum value
  if (_usageTrackingModeString == "enabled-per-shard") {
    _usageTrackingMode = UsageTrackingMode::kEnabledPerShard;
  } else if (_usageTrackingModeString == "enabled-per-shard-per-user") {
    _usageTrackingMode = UsageTrackingMode::kEnabledPerShardPerUser;
  } else {
    _usageTrackingMode = UsageTrackingMode::kDisabled;
  }
}

void MetricsFeature::toPrometheus(std::string& result,
                                  MetricsParts metricsParts,
                                  CollectMode mode) const {
  // minimize reallocs
  result.reserve(64 * 1024);

  if (metricsParts.includeStandardMetrics()) {
    // QueryRegistryFeature only provides standard metrics.
    // update only necessary if these metrics should be included
    // in the output
    _queryRegistryFeature->updateMetrics();
  }

  bool hasGlobals = false;
  {
    auto lock = initGlobalLabels();
    hasGlobals = hasShortname && hasRole;
    std::string_view last;
    std::string_view curr;
    for (auto const& i : _registry) {
      TRI_ASSERT(i.second);
      if (i.second->isDynamic()) {
        if (!metricsParts.includeDynamicMetrics()) {
          continue;
        }
      } else {
        if (!metricsParts.includeStandardMetrics()) {
          continue;
        }
      }
      curr = i.second->name();
      if (last != curr) {
        last = curr;
        Metric::addInfo(result, curr, i.second->help(), i.second->type());
      }
      i.second->toPrometheus(result, _globals, _ensureWhitespace);
    }
    for (auto const& [_, batch] : _batch) {
      TRI_ASSERT(batch);
      // TODO(MBkkt) merge vector::reserve's between IBatch::toPrometheus
      batch->toPrometheus(result, _globals, _ensureWhitespace);
    }
  }

  if (metricsParts.includeStandardMetrics()) {
    // StatisticsFeature only provides standard metrics
    auto time = std::chrono::duration<double, std::milli>(
        std::chrono::system_clock::now().time_since_epoch());
    _statisticsFeature->toPrometheus(result, time.count(), _globals,
                                     _ensureWhitespace);

    // Storage engine only provides standard metrics
    auto& es = _engineSelectorFeature->engine();
    if (es.typeName() == RocksDBEngine::kEngineName) {
      es.toPrometheus(result, _globals, _ensureWhitespace);
    }

    // ClusterMetricsFeature only provides standard metrics
    if (hasGlobals && _clusterMetricsFeature->isEnabled() &&
        mode != CollectMode::Local) {
      _clusterMetricsFeature->toPrometheus(result, _globals, _ensureWhitespace);
    }

    // agency node metrics only provide standard metrics
    consensus::Node::toPrometheus(result, _globals, _ensureWhitespace);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// Sets metrics that can be collected by ClusterMetricsFeature
////////////////////////////////////////////////////////////////////////////////
constexpr auto kCoordinatorBatch = frozen::make_unordered_set<frozen::string>({
    "arangodb_search_link_stats",
});

constexpr auto kCoordinatorMetrics =
    frozen::make_unordered_set<frozen::string>({
        "arangodb_search_num_failed_commits",
        "arangodb_search_num_failed_cleanups",
        "arangodb_search_num_failed_consolidations",
        "arangodb_search_commit_time",
        "arangodb_search_cleanup_time",
        "arangodb_search_consolidation_time",
    });

void MetricsFeature::toVPack(velocypack::Builder& builder,
                             MetricsParts metricsParts) const {
  builder.openArray(true);
  std::shared_lock lock{_mutex};
  for (auto const& i : _registry) {
    TRI_ASSERT(i.second);
    auto const name = i.second->name();
    if (kCoordinatorMetrics.count(name)) {
      i.second->toVPack(builder);
    }
  }
  auto& ci = _clusterFeature->clusterInfo();
  for (auto const& [name, batch] : _batch) {
    TRI_ASSERT(batch);
    if (kCoordinatorBatch.count(name)) {
      batch->toVPack(builder, ci);
    }
  }
  lock.unlock();
  builder.close();
}

ServerStatistics& MetricsFeature::serverStatistics() noexcept {
  return *_serverStatistics;
}

std::shared_lock<std::shared_mutex> MetricsFeature::initGlobalLabels() const {
  std::shared_lock sharedLock{_mutex};
  auto instance = ServerState::instance();
  if (!instance || (hasShortname && hasRole)) {
    return sharedLock;
  }
  sharedLock.unlock();
  std::unique_lock uniqueLock{_mutex};
  if (!hasShortname) {
    // Very early after a server start it is possible that the short name
    // isn't yet known. This check here is to prevent that the label is
    // permanently empty if metrics are requested too early.
    if (auto shortname = instance->getShortName(); !shortname.empty()) {
      _globals = absl::StrCat("shortname=\"", shortname, "\"",
                              (_globals.empty() ? "" : ","), _globals);
      hasShortname = true;
    }
  }
  if (!hasRole) {
    if (auto role = instance->getRole(); role != ServerState::ROLE_UNDEFINED) {
      absl::StrAppend(&_globals, (_globals.empty() ? "" : ","), "role=\"",
                      ServerState::roleToString(role), "\"");
      hasRole = true;
    }
  }
  uniqueLock.unlock();
  sharedLock.lock();
  return sharedLock;
}

std::pair<std::shared_lock<std::shared_mutex>, metrics::IBatch*>
MetricsFeature::getBatch(std::string_view name) const {
  std::shared_lock lock{_mutex};
  metrics::IBatch* batch = nullptr;
  if (auto it = _batch.find(name); it != _batch.end()) {
    batch = it->second.get();
  } else {
    lock.unlock();
    lock.release();
  }
  return {std::move(lock), batch};
}

void MetricsFeature::batchRemove(std::string_view name,
                                 std::string_view labels) {
  std::unique_lock lock{_mutex};
  auto it = _batch.find(name);
  if (it == _batch.end()) {
    return;
  }
  TRI_ASSERT(it->second);
  if (it->second->remove(labels) == 0) {
    _batch.erase(name);
  }
}

void MetricsFeature::prepare() {
  _queryRegistryFeature = std::move(_lazyQueryRegistryFeatureRef).get();
  _statisticsFeature = std::move(_lazyStatisticsFeatureRef).get();
  _engineSelectorFeature = std::move(_lazyEngineSelectorFeatureRef).get();
  _clusterMetricsFeature = std::move(_lazyClusterMetricsFeatureRef).get();
  _clusterFeature = std::move(_lazyClusterFeatureRef).get();
}

}  // namespace arangodb::metrics
