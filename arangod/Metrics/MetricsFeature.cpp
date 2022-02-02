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

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/GreetingsFeaturePhase.h"
#include "Basics/application-exit.h"
#include "Basics/debugging.h"
#include "Cluster/ServerState.h"
#include "Logger/LoggerFeature.h"
#include "Metrics/Metric.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "Statistics/StatisticsFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"

#include <chrono>

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
      ->addOption(
          "--server.export-metrics-api", "turn metrics API on or off",
          new options::BooleanParameter(&_export),
          arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden))
      .setIntroducedIn(30600);

  options
      ->addOption(
          "--server.export-read-write-metrics",
          "turn metrics for document read/write metrics on or off",
          new options::BooleanParameter(&_exportReadWriteMetrics),
          arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden))
      .setIntroducedIn(30707);
}

std::shared_ptr<Metric> MetricsFeature::doAdd(Builder& builder) {
  auto metric = builder.build();
  MetricKey key{metric->name(), metric->labels()};
  std::lock_guard lock{_mutex};
  if (!_registry.try_emplace(key, metric).second) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   std::string{builder.type()} +
                                       std::string{builder.name()} +
                                       " already exists");
  }
  return metric;
}

Metric* MetricsFeature::get(MetricKey const& key) {
  std::shared_lock lock{_mutex};
  auto it = _registry.find(key);
  if (it == _registry.end()) {
    return nullptr;
  }
  return it->second.get();
}

bool MetricsFeature::remove(Builder const& builder) {
  MetricKey key{builder.name(), builder.labels()};
  std::lock_guard guard{_mutex};
  return _registry.erase(key) != 0;
}

bool MetricsFeature::exportAPI() const { return _export; }

void MetricsFeature::validateOptions(std::shared_ptr<options::ProgramOptions>) {
  if (_exportReadWriteMetrics) {
    serverStatistics().setupDocumentMetrics();
  }
}

void MetricsFeature::toPrometheus(std::string& result) const {
  // minimize reallocs
  result.reserve(32768);

  // QueryRegistryFeature
  auto& q = server().getFeature<QueryRegistryFeature>();
  q.updateMetrics();
  initGlobalLabels();
  {
    std::shared_lock lock{_mutex};
    std::string_view last;
    std::string_view curr;
    for (auto const& i : _registry) {
      TRI_ASSERT(i.second);
      curr = i.second->name();
      bool const first = last != curr;
      if (first) {
        last = curr;
      }
      i.second->toPrometheus(result, first, _globals);
    }
  }
  auto& sf = server().getFeature<StatisticsFeature>();
  sf.toPrometheus(result,
                  std::chrono::duration<double, std::milli>(
                      std::chrono::system_clock::now().time_since_epoch())
                      .count());
  auto& es = server().getFeature<EngineSelectorFeature>().engine();
  if (es.typeName() == RocksDBEngine::kEngineName) {
    es.getStatistics(result);
  }
}

ServerStatistics& MetricsFeature::serverStatistics() noexcept {
  return *_serverStatistics;
}

void MetricsFeature::initGlobalLabels() const {
  auto instance = ServerState::instance();
  if (!instance) {
    return;
  }
  std::lock_guard lock{_mutex};
  if (!hasShortname) {
    // Very early after a server start it is possible that the short name isn't
    // yet known. This check here is to prevent that the label is permanently
    // empty if metrics are requested too early.
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
}

}  // namespace arangodb::metrics
