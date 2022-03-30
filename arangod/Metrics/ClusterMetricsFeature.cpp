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
/// @author Valery Mironov
////////////////////////////////////////////////////////////////////////////////

#include "Metrics/ClusterMetricsFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/debugging.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterMethods.h"
#include "Cluster/ServerState.h"
#include "Containers/FlatHashSet.h"
#include "Logger/LoggerFeature.h"
#include "Metrics/Metric.h"
#include "Metrics/MetricsFeature.h"
#include "Network/NetworkFeature.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Scheduler/SchedulerFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "Aql/QueryString.h"
#include "Aql/Query.h"
#include "Transaction/StandaloneContext.h"

namespace arangodb::metrics {
using namespace std::chrono_literals;

ClusterMetricsFeature::ClusterMetricsFeature(Server& server)
    : ArangodFeature{server, *this} {
  setOptional();
  startsAfter<SystemDatabaseFeature>();
  startsAfter<ClusterFeature>();
  startsAfter<NetworkFeature>();
  startsAfter<SchedulerFeature>();
  startsAfter<BootstrapFeature>();
}

void ClusterMetricsFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  options
      ->addOption("--server.cluster-metrics-timeout",
                  "Cluster metrics polling timeout in seconds",
                  new options::UInt32Parameter(&_timeout),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Uncommon))
      .setIntroducedIn(310000);
}

void ClusterMetricsFeature::validateOptions(
    std::shared_ptr<options::ProgramOptions> /*options*/) {
  if (!ServerState::instance()->isCoordinator()) {
    _count.store(kStop, std::memory_order_release);
  }
}

void ClusterMetricsFeature::start() {
  if (wasStop()) {
    return;
  }
  aql::QueryString writeText{
      absl::StrCat("INSERT { _key: \"metrics\", version: 0, data: [] } INTO ",
                   StaticStrings::MetricsCollection)};
  auto& sdf = server().getFeature<SystemDatabaseFeature>();
  auto vocbase = sdf.use();
  auto writeQuery = aql::Query::create(
      transaction::StandaloneContext::Create(*vocbase), writeText, nullptr);
  writeQuery->executeSync();

  update(CollectMode::ReadGlobal);
}

void ClusterMetricsFeature::beginShutdown() {
  _count.store(kStop, std::memory_order_release);
  _handle = nullptr;
}

void ClusterMetricsFeature::update(CollectMode mode) {
  if (mode == CollectMode::Local || wasStop()) {
    return;
  }
  switch (mode) {
    case CollectMode::WriteGlobal:
      [[fallthrough]];
    case CollectMode::ReadGlobal:
      try {
        uint64_t version = 0;
        auto result = readData(version);
        if (mode == CollectMode::WriteGlobal && result != Result::Error) {
          auto& nf = server().getFeature<NetworkFeature>();
          auto& cf = server().getFeature<ClusterFeature>();
          auto& ci = cf.clusterInfo();
          if (ci.startCollectMetrics()) {
            result = writeData(version, metricsOnCoordinator(nf, cf).getTry());
            ci.endCollectMetrics();
          } else {
            result = Result::Old;
          }
        }
        if (result == Result::New) {
          return;
        }
      } catch (...) {
      }
      [[fallthrough]];
    case CollectMode::TriggerGlobal:
      if (_count.fetch_add(kUpdate, std::memory_order_acq_rel) == 0) {
        rescheduleUpdate(_timeout);
      }
    default:
      break;
  }
}

void ClusterMetricsFeature::update() {
  if (wasStop()) {
    return;
  }
  // We want more time than _timeout should expire
  // after last try cross cluster communication
  _lastUpdate = std::chrono::steady_clock::now();
  uint64_t version = 0;
  auto result = readData(version);
  _lastUpdate = std::chrono::steady_clock::now();
  if (wasStop()) {
    return;
  }
  if (result == Result::Error) {
    return rescheduleUpdate(std::max(_timeout, uint32_t{1}));
  }
  if (result == Result::New) {
    return repeatUpdate();
  }
  auto& cf = server().getFeature<ClusterFeature>();
  auto& ci = cf.clusterInfo();
  if (!ci.startCollectMetrics()) {
    return rescheduleUpdate(std::max(_timeout, uint32_t{1}));
  }
  auto& nf = server().getFeature<NetworkFeature>();
  if (wasStop()) {
    return;
  }
  metricsOnCoordinator(nf, cf).thenFinal(
      [this, version, &ci](futures::Try<RawDBServers>&& raw) mutable {
        if (wasStop()) {
          return;
        }
        try {
          _lastUpdate = std::chrono::steady_clock::now();
          auto result = writeData(version, std::move(raw));
          _lastUpdate = std::chrono::steady_clock::now();
          ci.endCollectMetrics();
          _lastUpdate = std::chrono::steady_clock::now();
          if (result == Result::New) {
            return repeatUpdate();
          }
        } catch (...) {
        }
        if (wasStop()) {
          return;
        }
        rescheduleUpdate(std::max(_timeout, uint32_t{1}));
      });
}

ClusterMetricsFeature::Result ClusterMetricsFeature::readData(
    uint64_t& version) {
  auto& sdf = server().getFeature<SystemDatabaseFeature>();
  auto vocbase = sdf.use();
  aql::QueryString readText{
      absl::StrCat("FOR d IN ", StaticStrings::MetricsCollection, " RETURN d")};
  auto readQuery = aql::Query::create(
      transaction::StandaloneContext::Create(*vocbase), readText, nullptr);
  auto result = readQuery->executeSync();
  if (!result.ok()) {
    return Result::Error;
  }
  auto data = std::atomic_load_explicit(&_data, std::memory_order_acquire);
  auto metrics = result.data->slice();
  version = metrics.at(0).get("version").getNumber<uint64_t>();
  if ((data ? data->version : 0) < version) {
    auto raw = metrics.at(0).get("data");
    std::atomic_store_explicit(&_data, Data::fromVPack(version, raw),
                               std::memory_order_release);
    return Result::New;
  }
  return Result::Old;
}

ClusterMetricsFeature::Result ClusterMetricsFeature::writeData(
    uint64_t version, futures::Try<RawDBServers>&& raw) {
  auto& sdf = server().getFeature<SystemDatabaseFeature>();
  auto vocbase = sdf.use();
  if (!raw.hasValue()) {
    return Result::Error;
  }
  auto metrics = parse(std::move(*raw));
  if (metrics.values.empty()) {
    return Result::Error;
  }
  VPackBuilder builder;
  builder.openObject();
  builder.add("d", VPackSerialize{metrics});
  builder.add("v", VPackValue{(version + 1)});
  builder.close();
  aql::QueryString writeText{
      "REPLACE { _key: \"metrics\", data: @d, version: @v } INTO " +
      StaticStrings::MetricsCollection};
  std::shared_ptr<VPackBuilder> sharedDefault;
  std::shared_ptr<VPackBuilder> bindVars{sharedDefault, &builder};
  auto writeQuery = aql::Query::create(
      transaction::StandaloneContext::Create(*vocbase), writeText, bindVars);
  writeQuery->executeSync();
  std::atomic_store_explicit(
      &_data, std::make_shared<Data>(version + 1, std::move(metrics)),
      std::memory_order_release);
  return Result::New;
}

void ClusterMetricsFeature::repeatUpdate() noexcept {
  auto count = _count.fetch_sub(kUpdate, std::memory_order_acq_rel);
  if (count % 2 != kStop && count > kUpdate) {
    rescheduleUpdate(_timeout);
  }
}

void ClusterMetricsFeature::rescheduleUpdate(uint32_t timeout) noexcept {
  auto now = std::chrono::steady_clock::now();
  auto next = _lastUpdate + std::chrono::seconds{timeout};
  _handle = SchedulerFeature::SCHEDULER->queueDelayed(
      RequestLane::CLUSTER_INTERNAL, (now < next ? next - now : 0ns),
      [this](bool canceled) {
        if (canceled) {
          return;
        }
        if (_count.exchange(kUpdate, std::memory_order_acq_rel) % 2 == kStop) {
          // If someone call more than 1 billion asyncUpdate() before we execute
          // store we defer Stop to next try. But it's impossible,
          // so it's valid optimization: exchange + store instead of cas loop
          return _count.store(kStop, std::memory_order_release);
        }
        try {
          update();
        } catch (...) {
          if (!wasStop()) {
            rescheduleUpdate(std::max(_timeout, uint32_t{1}));
          }
        }
      });
}

ClusterMetricsFeature::Metrics ClusterMetricsFeature::parse(
    RawDBServers&& raw) const {
  Metrics metrics;
  for (std::shared_lock lock{_m}; auto const& payload : raw) {
    TRI_ASSERT(payload);
    velocypack::Slice slice{payload->data()};
    TRI_ASSERT(slice.isArray());
    auto const size = slice.length();
    TRI_ASSERT(size % 3 == 0);
    for (size_t i = 0; i != size; i += 3) {
      auto name = slice.at(i).stringView();
      auto labels = slice.at(i + 1);
      auto value = slice.at(i + 2);
      auto f = _toCoordinator.find(name);
      if (f != _toCoordinator.end()) {
        f->second(metrics, name, labels, value);
      }
    }
  }
  return metrics;
}

bool ClusterMetricsFeature::wasStop() const noexcept {
  return _count.load(std::memory_order_acquire) % 2 == kStop;
}

void ClusterMetricsFeature::add(std::string_view metric,
                                ToCoordinator toCoordinator) {
  if (wasStop()) {
    return;
  }
  std::lock_guard lock{_m};
  _toCoordinator[metric] = toCoordinator;
}

void ClusterMetricsFeature::add(std::string_view metric,
                                ToCoordinator toCoordinator,
                                ToPrometheus toPrometheus) {
  if (wasStop()) {
    return;
  }
  std::lock_guard lock{_m};
  _toCoordinator[metric] = toCoordinator;
  _toPrometheus[metric] = toPrometheus;
}

void ClusterMetricsFeature::toPrometheus(std::string& result,
                                         std::string_view globals) const {
  if (wasStop()) {
    return;
  }
  auto data = std::atomic_load_explicit(&_data, std::memory_order_acquire);
  if (!data) {
    return;
  }
  std::string_view metricName;
  std::shared_lock lock{_m};
  auto it = _toPrometheus.end();
  for (auto const& [key, value] : data->metrics.values) {
    if (metricName != key.first) {
      metricName = key.first;
      it = _toPrometheus.find(metricName);
      if (it != _toPrometheus.end()) {
        // TODO(MBkkt) read help and type from global constexpr map
        Metric::addInfo(result, metricName, "NO HELP", "gauge");
      }
    }
    if (it != _toPrometheus.end()) {
      it->second(result, globals, key, value);
    }
  }
}

std::shared_ptr<ClusterMetricsFeature::Data>
ClusterMetricsFeature::Data::fromVPack(uint64_t version, VPackSlice metrics) {
  if (!metrics.isArray()) {
    TRI_ASSERT(false);
    return {};
  }
  auto const size = metrics.length();
  if (size == 0) {
    return {};
  }
  if (size % 3 != 0) {
    TRI_ASSERT(false);
    return {};
  }
  auto data = std::make_shared<Data>(version);
  for (size_t i = 0; i != size; i += 3) {
    auto name = metrics.at(i).stringView();
    auto labels = metrics.at(i + 1).stringView();
    auto value = metrics.at(i + 2).getNumber<uint64_t>();
    data->metrics.values.emplace(MetricKey{name, labels}, MetricValue{value});
  }
  return data;
}

void ClusterMetricsFeature::Metrics::toVelocyPack(VPackBuilder& builder) const {
  builder.openArray();
  for (auto const& [key, value] : values) {
    builder.add(VPackValue{key.first});
    builder.add(VPackValue{key.second});
    std::visit([&](auto&& v) { builder.add(VPackValue{v}); }, value);
  }
  builder.close();
}

}  // namespace arangodb::metrics
