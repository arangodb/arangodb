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

#pragma once

#include <chrono>
#include <memory>
#include <shared_mutex>
#include <string>
#include <variant>
#include <vector>

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Basics/DownCast.h"
#include "Containers/FlatHashMap.h"
#include "Metrics/Batch.h"
#include "Metrics/Builder.h"
#include "Metrics/IBatch.h"
#include "Metrics/Metric.h"
#include "Metrics/MetricKey.h"
#include "Metrics/Parse.h"
#include "ProgramOptions/ProgramOptions.h"
#include "RestServer/arangod.h"
#include "Scheduler/SchedulerFeature.h"
#include "Statistics/ServerStatistics.h"

namespace arangodb::metrics {

////////////////////////////////////////////////////////////////////////////////
/// This feature is able to asynchronously collect metrics
/// from cluster DBServers and accumulate them on the Coordinators
////////////////////////////////////////////////////////////////////////////////
class ClusterMetricsFeature final : public ArangodFeature {
 public:
  static constexpr std::string_view name() noexcept { return "ClusterMetrics"; }

  // If you need to store a metric of another type,
  // such as double, or char, just add it to this variant
  using MetricValue = std::variant<uint64_t>;
  // We want map because of promtool format
  // Another option is hashmap<string, hashmap<string, value>
  using Metrics = std::map<MetricKey<std::string>, MetricValue, std::less<>>;

  struct Data {
    Data() = default;
    explicit Data(Metrics&& m) : metrics{std::move(m)} {}

    Metrics metrics;
  };

  explicit ClusterMetricsFeature(Server& server);

  void collectOptions(std::shared_ptr<options::ProgramOptions> options) final;
  void validateOptions(std::shared_ptr<options::ProgramOptions> options) final;
  void start() final;
  void beginShutdown() final;

  //////////////////////////////////////////////////////////////////////////////
  /// Attempt to start updating metrics. If it's happening right now, or the
  /// timeout hasn't passed since the last update, then we will postpone this
  /// update for the future. There will be no more than one scheduled update at
  /// a time, they will collapse.
  //////////////////////////////////////////////////////////////////////////////
  void asyncUpdate();

  //////////////////////////////////////////////////////////////////////////////
  /// ToCoordinator custom function that accumulate many metrics from DBServers
  /// to some metrics on Coordinators
  //////////////////////////////////////////////////////////////////////////////
  using ToCoordinator = void (*)(Metrics& /*reduced metrics*/,
                                 std::string_view /*metric name*/,
                                 velocypack::Slice /*metric labels*/,
                                 velocypack::Slice /*metric value*/);
  //////////////////////////////////////////////////////////////////////////////
  /// ToCoordinator custom function that accumulate many metrics from DBServers
  /// to some metrics on Coordinators
  //////////////////////////////////////////////////////////////////////////////
  using ToPrometheus = void (*)(std::string& /*result*/,
                                std::string_view /*global labels*/,
                                std::string_view /*metric name*/,
                                std::string_view /*metric labels*/,
                                MetricValue const&);

  //////////////////////////////////////////////////////////////////////////////
  /// Registration of some metric. We need to pass it name, and callbacks.
  ///
  /// @param toCoordinator used to accumulate metrics
  /// @param toPrometheus used to print accumulated metrics in Prometheus format
  //////////////////////////////////////////////////////////////////////////////
  void add(std::string_view metric, ToCoordinator toCoordinator,
           ToPrometheus toPrometheus);

  //////////////////////////////////////////////////////////////////////////////
  /// @see add but sometimes we want to accumulate some metrics from DBServers,
  /// but we don't want to make them public, so we don't need toPrometheus
  //////////////////////////////////////////////////////////////////////////////
  void add(std::string_view metric, ToCoordinator toCoordinator);

  void toPrometheus(std::string& result, std::string_view globals) const;

  std::shared_ptr<Data> getData() const;

 private:
  mutable std::shared_mutex _m;
  containers::FlatHashMap<std::string_view, ToCoordinator> _toCoordinator;
  containers::FlatHashMap<std::string_view, ToPrometheus> _toPrometheus;

  void scheduleUpdate() noexcept;
  void update();
  void repeatUpdate() noexcept;

  void set(RawDBServers&& metrics) const;

  mutable std::shared_ptr<Data> _data;
  Scheduler::WorkHandle _handle;
  std::chrono::time_point<std::chrono::steady_clock> _lastUpdate{};
  uint32_t _timeout{0};

  static constexpr uint32_t kStop = 1;
  static constexpr uint32_t kUpdate = 2;
  std::atomic_uint32_t _count{0};
};

}  // namespace arangodb::metrics
