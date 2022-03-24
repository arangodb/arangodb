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
    _count.store(kStop, std::memory_order_release);
  }
}

void ClusterMetricsFeature::start() { asyncUpdate(); }

void ClusterMetricsFeature::beginShutdown() {
  _count.store(kStop, std::memory_order_release);
  _handle = nullptr;
}

void ClusterMetricsFeature::asyncUpdate() {
  if (_count.load(std::memory_order_acquire) % 2 != kStop &&
      _count.fetch_add(kUpdate, std::memory_order_acq_rel) == 0) {
    scheduleUpdate();
  }
}

void ClusterMetricsFeature::scheduleUpdate() noexcept {
  auto now = std::chrono::steady_clock::now();
  auto next = _lastUpdate + std::chrono::seconds{_timeout};
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
          _count.store(kStop, std::memory_order_release);
          return;
        }
        try {
          update();
        } catch (...) {
          repeatUpdate();
        }
      });
}

void ClusterMetricsFeature::update() {
  auto& nf = server().getFeature<NetworkFeature>();
  auto& cf = server().getFeature<ClusterFeature>();
  // TODO try get from agency
  metricsOnCoordinator(nf, cf).thenFinal(
      [this](futures::Try<RawDBServers>&& metrics) mutable {
        // We want more time than kTimeout should expire
        // after last try cross cluster communication
        _lastUpdate = std::chrono::steady_clock::now();
        if (!metrics.hasValue()) {
          scheduleUpdate();
          return;
        }
        try {
          set(std::move(*metrics));
        } catch (...) {
        }
        repeatUpdate();
      });
}

void ClusterMetricsFeature::repeatUpdate() noexcept {
  auto count = _count.fetch_sub(kUpdate, std::memory_order_acq_rel);
  if (count % 2 != kStop && count > kUpdate) {
    scheduleUpdate();
  }
}

void ClusterMetricsFeature::set(RawDBServers&& raw) const {
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

  auto data = std::make_shared<Data>(std::move(metrics));
  std::atomic_store_explicit(&_data, data, std::memory_order_release);
  // TODO(MBkkt) serialize to velocypack array
  // TODO(MBkkt) set to agency
}

void ClusterMetricsFeature::add(std::string_view metric,
                                ToCoordinator toCoordinator) {
  if (_count.load(std::memory_order_acquire) % 2 == kStop) {
    return;
  }
  std::lock_guard lock{_m};
  _toCoordinator[metric] = toCoordinator;
}

void ClusterMetricsFeature::add(std::string_view metric,
                                ToCoordinator toCoordinator,
                                ToPrometheus toPrometheus) {
  if (_count.load(std::memory_order_acquire) % 2 == kStop) {
    return;
  }
  std::lock_guard lock{_m};
  _toCoordinator[metric] = toCoordinator;
  _toPrometheus[metric] = toPrometheus;
}

void ClusterMetricsFeature::toPrometheus(std::string& result,
                                         std::string_view globals) const {
  if (_count.load(std::memory_order_acquire) % 2 == kStop) {
    return;
  }
  auto data = std::atomic_load_explicit(&_data, std::memory_order_acquire);
  if (!data) {
    return;
  }
  std::shared_lock lock{_m};
  std::string_view metricName;
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

}  // namespace arangodb::metrics
