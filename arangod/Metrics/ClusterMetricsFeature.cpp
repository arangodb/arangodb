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
  auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
  ci.initMetricsUpdate();
  aql::QueryString writeText{
      absl::StrCat("UPSERT { _key: \"metrics\" }"
                   " INSERT { _key: \"metrics\", version: 0, data: [] }"
                   " UPDATE { version: 0, data: [] } IN ",
                   StaticStrings::MetricsCollection)};
  // TODO(MBkkt) We should save old value for persistent metrics
  auto vocbase = server().getFeature<SystemDatabaseFeature>().use();
  transaction::StandaloneContext context{*vocbase};
  auto writeQuery = aql::Query::create(
      {std::shared_ptr<transaction::StandaloneContext>{}, &context}, writeText,
      nullptr);
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
          result = writeData(version, metricsOnCoordinator(nf, cf).getTry());
          if (result == Result::Old) {
            // Now there is new data in the global cache,
            // relative to what we read at the start of this operation,
            // so we are trying to read it into the local cache.
            result = readData(version);
            // We're also happy with Result::Old
            // because it means that someone else has updated the local cache.
            if (result == Result::Old) {
              return;
            }
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
  // We want more time than _timeout should expire
  // after last try cross cluster communication
  _lastUpdate = std::chrono::steady_clock::now();
  uint64_t version = 0;
  auto result = readData(version);
  if (wasStop()) {
    return;
  }
  _lastUpdate = std::chrono::steady_clock::now();
  if (result == Result::Error) {
    return rescheduleUpdate(std::max(_timeout, uint32_t{1}));
  }
  if (result == Result::New) {
    return repeatUpdate();
  }
  auto& cf = server().getFeature<ClusterFeature>();
  auto& ci = cf.clusterInfo();
  if (_acquiredUpdateLock) {
    _acquiredUpdateLock = !ci.unlockMetricsUpdate();
  }
  if (_acquiredUpdateLock) {
    return rescheduleUpdate(std::max(_timeout, uint32_t{1}));
  }
  _acquiredUpdateLock = ci.lockMetricsUpdate();
  if (!_acquiredUpdateLock) {
    return rescheduleUpdate(std::max(_timeout, uint32_t{1}));
  }
  if (wasStop()) {
    return;
  }
  auto& nf = server().getFeature<NetworkFeature>();
  metricsOnCoordinator(nf, cf).thenFinal(
      [this, version, &ci](futures::Try<RawDBServers>&& raw) mutable {
        if (wasStop()) {
          return;
        }
        auto result = Result::Old;
        try {
          _lastUpdate = std::chrono::steady_clock::now();
          result = writeData(version, std::move(raw));
          _lastUpdate = std::chrono::steady_clock::now();
        } catch (...) {
          result = Result::Error;
        }
        _acquiredUpdateLock = !ci.unlockMetricsUpdate();
        _lastUpdate = std::chrono::steady_clock::now();
        if (result != Result::Error) {
          LOG_TOPIC("badf1", INFO, Logger::CLUSTER)
              << "Successful collect and store metrics";
          repeatUpdate();
        }
        rescheduleUpdate(std::max(_timeout, uint32_t{1}));
      });
}

ClusterMetricsFeature::Result ClusterMetricsFeature::readData(
    uint64_t& version) {
  auto vocbase = server().getFeature<SystemDatabaseFeature>().use();
  aql::QueryString readText{
      absl::StrCat("FOR m IN ", StaticStrings::MetricsCollection, " RETURN m")};
  transaction::StandaloneContext context{*vocbase};
  auto readQuery = aql::Query::create(
      {std::shared_ptr<transaction::StandaloneContext>{}, &context}, readText,
      nullptr);
  auto r = readQuery->executeSync();
  if (r.fail()) {
    return Result::Error;
  }
  auto metrics = r.data->slice().at(0);
  version = metrics.get("version").getNumber<uint64_t>();
  auto data = std::atomic_load_explicit(&_data, std::memory_order_acquire);
  if ((data ? data->version : 0) < version) {
    auto raw = metrics.get("data");
    std::atomic_store_explicit(&_data, Data::fromVPack(version, raw),
                               std::memory_order_release);
    return Result::New;
  }
  return Result::Old;
}

ClusterMetricsFeature::Result ClusterMetricsFeature::writeData(
    uint64_t version, futures::Try<RawDBServers>&& raw) {
  auto vocbase = server().getFeature<SystemDatabaseFeature>().use();
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
  builder.add("v", VPackValue{version + 1});
  builder.close();
  aql::QueryString writeText{
      absl::StrCat("FOR m IN ", StaticStrings::MetricsCollection,
                   " FILTER m.version < @v"
                   " REPLACE m WITH { data: @d, version: @v } IN _metrics"
                   " RETURN 0")};
  std::shared_ptr<VPackBuilder> sharedDefault;
  std::shared_ptr<VPackBuilder> bindVars{sharedDefault, &builder};
  transaction::StandaloneContext context{*vocbase};
  auto writeQuery = aql::Query::create(
      {std::shared_ptr<transaction::StandaloneContext>{}, &context}, writeText,
      bindVars);
  auto r = writeQuery->executeSync();
  if (r.fail()) {
    return Result::Error;
  }
  if (r.data->slice().length() == 0) {
    return Result::Old;
  }
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
  if (wasStop()) {
    return;
  }
  auto next = _lastUpdate + std::chrono::seconds{timeout};
  auto now = std::chrono::steady_clock::now();
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
          rescheduleUpdate(std::max(_timeout, uint32_t{1}));
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
  auto const size = metrics.length();
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
