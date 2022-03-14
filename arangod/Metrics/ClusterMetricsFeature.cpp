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
#include "RestServer/QueryRegistryFeature.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "Scheduler/SchedulerFeature.h"
#include "Statistics/StatisticsFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"

namespace arangodb::metrics {
using namespace std::chrono_literals;

ClusterMetricsFeature::ClusterMetricsFeature(Server& server)
    : ArangodFeature{server, *this} {
  setOptional();
  startsAfter<ClusterFeature>();
  startsAfter<NetworkFeature>();
  startsAfter<SchedulerFeature>();
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
    disable();
  }
}

void ClusterMetricsFeature::start() {
  if (!isEnabled()) {
    return;
  }
  auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
  ci.initMetrics();
  asyncUpdate();
}

void ClusterMetricsFeature::beginShutdown() {
  if (!isEnabled()) {
    return;
  }
  disable();
  _count.store(std::numeric_limits<int32_t>::min() + 1,
               std::memory_order_relaxed);
  _handle = nullptr;
}

void ClusterMetricsFeature::asyncUpdate() {
  if (isEnabled() && _count.fetch_add(1, std::memory_order_acq_rel) == 0) {
    rescheduleUpdate(_timeout);
  }
}

void ClusterMetricsFeature::rescheduleUpdate(uint32_t timeout) noexcept {
  auto now = std::chrono::steady_clock::now();
  auto next = _lastUpdate + std::chrono::seconds{timeout};
  _handle = SchedulerFeature::SCHEDULER->queueDelayed(
      RequestLane::CLUSTER_INTERNAL, (now < next ? next - now : 0ns),
      [this](bool canceled) {
        if (canceled || _count.exchange(1, std::memory_order_relaxed) < 1) {
          return;
        }

        try {
          update();
        } catch (...) {
          rescheduleUpdate(std::max(_timeout, uint32_t{1}));
        }
      });
}

void ClusterMetricsFeature::update() {
  // We want more time than _timeout should expire
  // after last try cross cluster communication
  _lastUpdate = std::chrono::steady_clock::now();
  auto& cf = server().getFeature<ClusterFeature>();
  auto data = std::atomic_load_explicit(&_data, std::memory_order_acquire);
  auto r = cf.clusterInfo().startCollectMetrics(data);
  if (data) {
    std::atomic_store_explicit(&_data, std::move(data),
                               std::memory_order_release);
  }

  _lastUpdate = std::chrono::steady_clock::now();
  if (r == ClusterInfo::MetricsResult::Repeat) {
    return repeatUpdate();
  }
  if (r == ClusterInfo::MetricsResult::Reschedule) {
    return rescheduleUpdate(std::max(_timeout, uint32_t{1}));
  }

  auto& nf = server().getFeature<NetworkFeature>();
  metricsOnCoordinator(nf, cf).thenFinal(
      [this](futures::Try<RawDBServers>&& raw) mutable {
        _lastUpdate = std::chrono::steady_clock::now();
        if (!raw.hasValue()) {
          return rescheduleUpdate(std::max(_timeout, uint32_t{1}));
        }

        try {
          auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
          auto metrics = parse(std::move(*raw));
          if (metrics.empty()) {
            ci.endCollectMetrics(VPackSlice::noneSlice());  // unlock
            return repeatUpdate();
          }
          VPackBuilder builder;
          metrics::ClusterMetricsFeature::Data::toVPack(metrics, builder);
          auto version = ci.endCollectMetrics(builder.slice());
          if (version == 0) {
            return rescheduleUpdate(_timeout);
          }
          std::atomic_store_explicit(
              &_data, std::make_shared<Data>(version, std::move(metrics)),
              std::memory_order_release);
        } catch (...) {
          return rescheduleUpdate(std::max(_timeout, uint32_t{1}));
        }

        repeatUpdate();
      });
}

void ClusterMetricsFeature::repeatUpdate() noexcept {
  if (_count.fetch_sub(1, std::memory_order_acq_rel) > 1) {
    rescheduleUpdate(_timeout);
  }
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

void ClusterMetricsFeature::add(std::string_view metric,
                                ToCoordinator toCoordinator) {
  if (!isEnabled()) {
    return;
  }
  std::lock_guard lock{_m};
  _toCoordinator[metric] = toCoordinator;
}

void ClusterMetricsFeature::add(std::string_view metric,
                                ToCoordinator toCoordinator,
                                ToPrometheus toPrometheus) {
  if (!isEnabled()) {
    return;
  }
  std::lock_guard lock{_m};
  _toCoordinator[metric] = toCoordinator;
  _toPrometheus[metric] = toPrometheus;
}

void ClusterMetricsFeature::toPrometheus(std::string& result,
                                         std::string_view globals) const {
  if (!isEnabled()) {
    return;
  }
  auto data = std::atomic_load_explicit(&_data, std::memory_order_acquire);
  if (!data) {
    return;
  }
  std::string_view metricName;
  std::shared_lock lock{_m};
  auto it = _toPrometheus.end();
  for (auto const& [key, value] : data->metrics) {
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
ClusterMetricsFeature::Data::fromVPack(VPackSlice slice) {
  auto metrics = slice.get("Data");
  auto version = slice.get("Version").getUInt();
  if (!metrics.isArray()) {
    TRI_ASSERT(false);
    return {};
  }
  auto const size = metrics.length();
  if (size % 3 != 0) {
    TRI_ASSERT(false);
    return {};
  }
  auto data = std::make_shared<Data>(version);
  for (size_t i = 0; i != size; i += 3) {
    auto name = metrics.at(i).stringView();
    auto labels = metrics.at(i + 1).stringView();
    auto type = metrics.at(i + 2).type();
    MetricValue value;
    if (type == VPackValueType::UInt || type == VPackValueType::SmallInt) {
      value = metrics.at(i + 2).getUInt();
    } else {
      TRI_ASSERT(false);
      return {};
    }
    data->metrics.emplace(MetricKey{name, labels}, value);
  }
  return data;
}

void ClusterMetricsFeature::Data::toVPack(Metrics const& metrics,
                                          VPackBuilder& builder) {
  builder.openArray();
  for (auto const& [key, value] : metrics) {
    builder.add(VPackValue{key.first});
    builder.add(VPackValue{key.second});
    std::visit([&](auto&& v) { builder.add(VPackValue{v}); }, value);
  }
  builder.close();
}

}  // namespace arangodb::metrics
