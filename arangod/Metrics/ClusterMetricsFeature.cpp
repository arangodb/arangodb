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
#include "Transaction/StandaloneContext.h"

namespace arangodb::metrics {

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
      .setIntroducedIn(3'10'00);
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
  auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
  ci.initMetricsState();
  if (_timeout != 0) {
    rescheduleTimer();
  }
}

void ClusterMetricsFeature::beginShutdown() {
  _count.store(kStop, std::memory_order_release);
  std::atomic_store_explicit(&_timer, {}, std::memory_order_relaxed);
  std::atomic_store_explicit(&_update, {}, std::memory_order_relaxed);
}

void ClusterMetricsFeature::stop() { beginShutdown(); }

std::optional<std::string> ClusterMetricsFeature::update(
    CollectMode mode) noexcept {
  if (mode == CollectMode::TriggerGlobal) {
    auto const count = _count.fetch_add(kUpdate, std::memory_order_acq_rel);
    if (count == 0) {
      rescheduleUpdate(0);
    }
    return std::nullopt;
  }
  TRI_ASSERT(mode != CollectMode::Local);
  auto& nf = server().getFeature<NetworkFeature>();
  auto& cf = server().getFeature<ClusterFeature>();
  auto& ci = cf.clusterInfo();
  try {
    // TODO(MBkkt) Need to return Future<std::optional<std::string>>:
    // 1) other CollectModes: invalidFuture()
    // 2) follower:           makeFuture(leader)
    // 3) leader:             metricsOnLeader(...).thenValue(...)
    auto [leader, version] = ci.getMetricsState(false);
    if (leader) {
      return leader;
    }
    if (!leader && mode == CollectMode::WriteGlobal) {
      writeData(version, metricsOnLeader(nf, cf).getTry());
    }
  } catch (...) {
  }
  return std::nullopt;
}

void ClusterMetricsFeature::rescheduleTimer() noexcept {
  auto h = SchedulerFeature::SCHEDULER->queueDelayed(
      RequestLane::DELAYED_FUTURE, std::chrono::seconds{_timeout},
      [this](bool canceled) noexcept {
        if (canceled || wasStop()) {
          return;
        }
        update(CollectMode::TriggerGlobal);
        rescheduleTimer();
      });
  std::atomic_store_explicit(&_timer, std::move(h), std::memory_order_relaxed);
}

void ClusterMetricsFeature::rescheduleUpdate(uint32_t timeout) noexcept {
  auto h = SchedulerFeature::SCHEDULER->queueDelayed(
      RequestLane::CLUSTER_INTERNAL, std::chrono::seconds{timeout},
      [this](bool canceled) noexcept {
        if (canceled || wasStop()) {
          return;
        }
        if (_count.exchange(kUpdate, std::memory_order_acq_rel) % 2 == kStop) {
          // If someone call more than billion update(TriggerGlobal)
          // before we execute execute store we defer Stop to next try.
          // But it's impossible, so it's valid optimization:
          // exchange + store instead of cas loop
          _count.store(kStop, std::memory_order_release);
          return;
        }
        try {
          update();
        } catch (...) {
          repeatUpdate(true);
        }
      });
  std::atomic_store_explicit(&_update, std::move(h), std::memory_order_relaxed);
}

void ClusterMetricsFeature::update() {
  auto& nf = server().getFeature<NetworkFeature>();
  auto& cf = server().getFeature<ClusterFeature>();
  auto& ci = cf.clusterInfo();
  auto [leader, version] = ci.getMetricsState(true);
  if (wasStop()) {
    return;
  }
  if (!leader) {  // cannot read leader from agency, so assume it's leader
    return metricsOnLeader(nf, cf).thenFinal(
        [this, v = version](futures::Try<RawDBServers>&& raw) mutable noexcept {
          if (wasStop()) {
            return;
          }
          bool force = false;
          try {
            force = !writeData(v, std::move(raw));
          } catch (...) {
            force = true;
          }
          repeatUpdate(force);
        });
  }
  auto const oldVersion = [&] {
    auto data = getData();
    return data ? data->version : 0;
  }();
  if (leader->empty() || version <= oldVersion) {
    return rescheduleUpdate(std::max(_timeout, 1U));
  }
  metricsFromLeader(nf, cf, *leader)
      .thenFinal([this](futures::Try<LeaderResponse>&& raw) mutable noexcept {
        if (wasStop()) {
          return;
        }
        bool force = false;
        try {
          force = !readData(std::move(raw));
        } catch (...) {
          force = true;
        }
        repeatUpdate(force);
      });
}

void ClusterMetricsFeature::repeatUpdate(bool force) noexcept {
  if (force) {
    if (!wasStop()) {
      rescheduleUpdate(std::max(_timeout, 1U));
    }
  } else {
    auto const count = _count.fetch_sub(kUpdate, std::memory_order_acq_rel);
    if (count % 2 != kStop && count > kUpdate) {
      rescheduleUpdate(0);
    }
  }
}

bool ClusterMetricsFeature::writeData(uint64_t version,
                                      futures::Try<RawDBServers>&& raw) {
  if (!raw.hasValue()) {
    return false;
  }
  auto metrics = parse(std::move(raw).get());
  if (metrics.values.empty()) {
    return true;
  }
  velocypack::Builder builder;
  builder.openObject();
  builder.add("version", VPackValue{version + 1});
  builder.add(VPackValue{"data"});
  metrics.toVelocyPack(builder);
  builder.close();
  auto data = std::make_shared<Data>(version + 1, std::move(metrics));
  data->packed = builder.buffer();
  auto& cf = server().getFeature<ClusterFeature>();
  auto& ci = cf.clusterInfo();
  if (ci.tryIncMetricsVersion(version)) {
    std::atomic_store_explicit(&_data, std::move(data),
                               std::memory_order_release);
  }
  return true;
}

bool ClusterMetricsFeature::readData(futures::Try<LeaderResponse>&& raw) {
  if (!raw.hasValue() || !raw.get()) {
    return false;
  }
  velocypack::Slice metrics{raw.get()->data()};
  if (!metrics.isObject()) {
    return false;
  }
  auto const newVersion = metrics.get("version").getNumber<uint64_t>();
  auto const oldVersion = [&] {
    auto data = getData();
    return data ? data->version : 0;
  }();
  if (newVersion <= oldVersion) {
    return true;
  }
  auto data = Data::fromVPack(metrics);
  data->packed = std::move(raw).get();
  std::atomic_store_explicit(&_data, std::move(data),
                             std::memory_order_release);
  return true;
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
    for (size_t i = 0; i < size; i += 3) {
      auto name = slice.at(i).stringView();
      auto labels = slice.at(i + 1);
      auto value = slice.at(i + 2);
      auto f = _mapReduce.find(name);
      if (f != _mapReduce.end()) {
        f->second(metrics, name, labels, value);
      }
    }
  }
  return metrics;
}

bool ClusterMetricsFeature::wasStop() const noexcept {
  return _count.load(std::memory_order_acquire) % 2 == kStop ||
         server().isStopping();
}

void ClusterMetricsFeature::add(std::string_view metric, MapReduce mapReduce) {
  std::lock_guard lock{_m};
  _mapReduce[metric] = mapReduce;
}

void ClusterMetricsFeature::add(std::string_view metric, MapReduce mapReduce,
                                ToPrometheus toPrometheus) {
  std::lock_guard lock{_m};
  _mapReduce[metric] = mapReduce;
  _toPrometheus[metric] = toPrometheus;
}

void ClusterMetricsFeature::toPrometheus(std::string& result,
                                         std::string_view globals) const {
  auto data = getData();
  if (!data) {
    return;
  }
  std::string_view metricName;
  std::shared_lock lock{_m};
  auto it = _toPrometheus.end();
  for (auto const& [key, value] : data->metrics.values) {
    if (metricName != key.name) {
      metricName = key.name;
      it = _toPrometheus.find(metricName);
      if (it != _toPrometheus.end()) {
        // TODO(MBkkt) read help and type from global constexpr map
        Metric::addInfo(result, metricName, "NO HELP", "gauge");
      }
    }
    if (it != _toPrometheus.end()) {
      it->second(result, globals, metricName, key.labels, value);
    }
  }
}

std::shared_ptr<ClusterMetricsFeature::Data> ClusterMetricsFeature::getData()
    const {
  return std::atomic_load_explicit(&_data, std::memory_order_acquire);
}

std::shared_ptr<ClusterMetricsFeature::Data>
ClusterMetricsFeature::Data::fromVPack(VPackSlice slice) {
  auto version = slice.get("version").getNumber<uint64_t>();
  auto metrics = slice.get("data");
  auto const size = metrics.length();
  auto data = std::make_shared<Data>(version);
  for (size_t i = 0; i < size; i += 3) {
    auto name = metrics.at(i).stringView();
    auto labels = metrics.at(i + 1).stringView();
    auto value = metrics.at(i + 2).getNumber<uint64_t>();
    data->metrics.values.emplace(
        MetricKey<std::string>{std::string{name}, std::string{labels}},
        MetricValue{value});
  }
  return data;
}

void ClusterMetricsFeature::Metrics::toVelocyPack(VPackBuilder& builder) const {
  builder.openArray();
  for (auto const& [key, value] : values) {
    builder.add(VPackValue{key.name});
    builder.add(VPackValue{key.labels});
    std::visit([&](auto&& v) { builder.add(VPackValue{v}); }, value);
  }
  builder.close();
}

}  // namespace arangodb::metrics
