////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////
#include "Metrics/MetricsFeature.h"

#include <frozen/unordered_set.h>
#include <velocypack/Builder.h>

#include <chrono>

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/GreetingsFeaturePhase.h"
#include "Basics/application-exit.h"
#include "Basics/debugging.h"
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

MetricsFeature::MetricsFeature(Server& server)
    : ArangodFeature{server, *this},
      _export{true},
      _exportReadWriteMetrics{false} {
  setOptional(false);
  startsAfter<LoggerFeature>();
  startsBefore<application_features::GreetingsFeaturePhase>();
}

void MetricsFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  _serverStatistics =
      std::make_unique<ServerStatistics>(*this, StatisticsFeature::time());

  options
      ->addOption("--server.export-metrics-api", "turn metrics API on or off",
                  new options::BooleanParameter(&_export),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Uncommon))
      .setIntroducedIn(30600);

  options
      ->addOption("--server.export-read-write-metrics",
                  "turn metrics for document read/write metrics on or off",
                  new options::BooleanParameter(&_exportReadWriteMetrics),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Uncommon))
      .setIntroducedIn(30707);
}

std::shared_ptr<Metric> MetricsFeature::doAdd(Builder& builder) {
  auto metric = builder.build();
  MetricKeyView key{metric->name(), metric->labels()};
  std::lock_guard lock{_mutex};
  if (!_registry.try_emplace(key, metric).second) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   std::string{builder.type()} + " " +
                                       std::string{builder.name()} +
                                       " already exists");
  }
  return metric;
}

Metric* MetricsFeature::get(MetricKeyView const& key) {
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

bool MetricsFeature::exportAPI() const { return _export; }

void MetricsFeature::validateOptions(std::shared_ptr<options::ProgramOptions>) {
  if (_exportReadWriteMetrics) {
    serverStatistics().setupDocumentMetrics();
  }
}

void MetricsFeature::toPrometheus(std::string& result, CollectMode mode) const {
  // minimize reallocs
  result.reserve(32768);

  // QueryRegistryFeature
  auto& q = server().getFeature<QueryRegistryFeature>();
  q.updateMetrics();
  bool hasGlobals = false;
  {
    auto lock = initGlobalLabels();
    hasGlobals = hasShortname && hasRole;
    std::string_view last;
    std::string_view curr;
    for (auto const& i : _registry) {
      TRI_ASSERT(i.second);
      curr = i.second->name();
      if (last != curr) {
        last = curr;
        Metric::addInfo(result, curr, i.second->help(), i.second->type());
      }
      i.second->toPrometheus(result, _globals);
    }
    for (auto const& [_, batch] : _batch) {
      TRI_ASSERT(batch);
      // TODO(MBkkt) merge vector::reserve's between IBatch::toPrometheus
      batch->toPrometheus(result, _globals);
    }
  }
  auto& sf = server().getFeature<StatisticsFeature>();
  auto time = std::chrono::duration<double, std::milli>(
      std::chrono::system_clock::now().time_since_epoch());
  sf.toPrometheus(result, time.count());
  auto& es = server().getFeature<EngineSelectorFeature>().engine();
  if (es.typeName() == RocksDBEngine::kEngineName) {
    es.getStatistics(result);
  }
  auto& cm = server().getFeature<ClusterMetricsFeature>();
  if (hasGlobals && cm.isEnabled() && mode != CollectMode::Local) {
    cm.toPrometheus(result, _globals);
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

void MetricsFeature::toVPack(velocypack::Builder& builder) const {
  builder.openArray(true);
  std::shared_lock lock{_mutex};
  for (auto const& i : _registry) {
    TRI_ASSERT(i.second);
    auto const name = i.second->name();
    if (kCoordinatorMetrics.count(name)) {
      i.second->toVPack(builder, server());
    }
  }
  for (auto const& [name, batch] : _batch) {
    TRI_ASSERT(batch);
    if (kCoordinatorBatch.count(name)) {
      batch->toVPack(builder, server());
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
      auto label = "shortname=\"" + shortname + "\"";
      _globals = label + (_globals.empty() ? "" : "," + _globals);
      hasShortname = true;
    }
  }
  if (!hasRole) {
    if (auto role = instance->getRole(); role != ServerState::ROLE_UNDEFINED) {
      auto label = "role=\"" + ServerState::roleToString(role) + "\"";
      _globals += (_globals.empty() ? "" : ",") + label;
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

}  // namespace arangodb::metrics
