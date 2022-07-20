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
    _count.store(kStop);
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
  _count.store(kStop);
  std::atomic_store_explicit(&_timer, {}, std::memory_order_relaxed);
  std::atomic_store_explicit(&_update, {}, std::memory_order_relaxed);
}

void ClusterMetricsFeature::stop() { beginShutdown(); }

std::optional<std::string> ClusterMetricsFeature::update(
    CollectMode mode) noexcept {
  if (mode == CollectMode::TriggerGlobal) {
    auto const count = _count.fetch_add(kUpdate /*== 2*/);
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
    auto leader = std::move(ci.getMetricsState(false).leader);
    if (leader) {
      return leader;
    }
    if (!leader && mode == CollectMode::WriteGlobal) {
      auto const version = [&] {
        auto data = getData();
        return (data && data->packed) ? VPackSlice{data->packed->data()}
                                            .get("Version")
                                            .getNumber<uint64_t>()
                                      : 0;
      }();
      writeData(version, metricsOnLeader(nf, cf).getTry());
    }
  } catch (...) {
  }
  return std::nullopt;
}

void ClusterMetricsFeature::rescheduleTimer() noexcept {
  TRI_ASSERT(_timeout > 0);
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
        if (_count.exchange(kUpdate) % 2 == kStop) {
          // If someone call more than billion update(TriggerGlobal)
          // before we execute execute store we defer Stop to next try.
          // But it's impossible, so it's valid optimization:
          // exchange + store instead of cas loop
          _count.store(kStop);
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
  auto leader = std::move(ci.getMetricsState(true).leader);
  auto data = getData();
  bool const isData = data && data->packed;
  auto const oldData =
      isData ? VPackSlice{data->packed->data()} : VPackSlice::noneSlice();
  auto version = isData ? oldData.get("Version").getNumber<uint64_t>() : 0;
  if (wasStop()) {
    return;
  }
  if (!leader) {  // cannot read leader from agency, so assume it's leader
    return metricsOnLeader(nf, cf).thenFinal(
        [this, version](futures::Try<RawDBServers>&& raw) mutable noexcept {
          if (wasStop()) {
            return;
          }
          bool force = false;
          try {
            force = !writeData(version, std::move(raw));
          } catch (...) {
            force = true;
          }
          repeatUpdate(force);
        });
  }
  if (leader->empty()) {
    return rescheduleUpdate(std::max(_timeout, 1U));
  }
  auto rebootId = isData ? oldData.get("RebootId").getNumber<uint64_t>() : 0;
  // We use `"0"` instead of `""` because we cannot parse empty string parameter
  auto serverId = isData ? oldData.get("ServerId").copyString() : "0";
  data.reset();
  metricsFromLeader(nf, cf, *leader, std::move(serverId), rebootId, version)
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
    auto const count = _count.fetch_sub(kUpdate);
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
  builder.add("ServerId", VPackValue{ServerState::instance()->getId()});
  builder.add("RebootId",
              VPackValue{ServerState::instance()->getRebootId().value()});
  builder.add("Version", VPackValue{version + 1});
  metrics.toVelocyPack(builder);
  builder.close();
  auto data = std::make_shared<Data>(std::move(metrics));
  data->packed = builder.buffer();
  std::atomic_store_explicit(&_data, std::move(data),
                             std::memory_order_release);
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
  return _count.load() % 2 == kStop || server().isStopping();
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
  auto const metrics = slice.get("Data");
  auto const size = metrics.length();
  auto data = std::make_shared<Data>();
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
  builder.add("Data", velocypack::ValueType::Array);
  for (auto const& [key, value] : values) {
    builder.add(VPackValue{key.name});
    builder.add(VPackValue{key.labels});
    std::visit([&](auto&& v) { builder.add(VPackValue{v}); }, value);
  }
  builder.close();
}

}  // namespace arangodb::metrics
