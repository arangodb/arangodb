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
#include "Metrics/CollectMode.h"
#include "Metrics/IBatch.h"
#include "Metrics/Metric.h"
#include "Metrics/MetricKey.h"
#include "Metrics/Parse.h"
#include "ProgramOptions/ProgramOptions.h"
#include "RestServer/arangod.h"
#include "Scheduler/Scheduler.h"
#include "Statistics/ServerStatistics.h"

namespace arangodb::metrics {

////////////////////////////////////////////////////////////////////////////////
/// This feature is able to asynchronously collect
/// metrics from cluster DBServers (MetricsFeature)
/// and accumulate them on the Coordinators (ClusterMetricsFeature)
///
/// See IResearchLinkCoordinator as an example
////////////////////////////////////////////////////////////////////////////////
class ClusterMetricsFeature final : public ArangodFeature {
 public:
  static constexpr std::string_view name() noexcept { return "ClusterMetrics"; }

  // If you need to store a metric of another type,
  // such as double, or char, just add it to this variant
  using MetricValue = std::variant<uint64_t>;

  struct Metrics final {
    // We want map because of promtool format
    // Another option is hashmap<string, hashmap<string, value>Ю
    using Values = std::map<MetricKey<std::string>, MetricValue, std::less<>>;

    Values values;

    void toVelocyPack(VPackBuilder& builder) const;

    template<typename T>
    T get(std::string_view key, std::string_view labels) const {
      auto it = values.find(MetricKeyView{key, labels});
      if (it != values.end()) {
        return std::get<T>(it->second);
      }
      return {};
    }
  };

  struct Data final {
    Data() = default;
    explicit Data(Metrics&& m) : metrics{std::move(m)} {}

    static std::shared_ptr<Data> fromVPack(VPackSlice slice);

    LeaderResponse packed;
    Metrics metrics;
  };
  explicit ClusterMetricsFeature(Server& server);

  void collectOptions(std::shared_ptr<options::ProgramOptions> options) final;
  void validateOptions(std::shared_ptr<options::ProgramOptions> options) final;
  void start() final;
  void beginShutdown() final;
  void stop() final;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Update current local cache of cluster metrics
  /// @param mode customize update behavior
  /// if mode is TriggerGlobal then async update and return nullopt
  /// other modes if we aren't leader return who is leader
  /// (RestHandler will make redirect to it)
  /// if we are leader then if mode is ReadGlobal just do nothing
  /// if mode is WriteGlobal then make blocking update from DB Servers
  /// @return leader server id, nullopt means no need return (TriggerGlobal)
  /// or current server is leader (other modes)
  //////////////////////////////////////////////////////////////////////////////
  std::optional<std::string> update(CollectMode mode) noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// ToCoordinator custom function that accumulate many metrics from DBServers
  /// to some metrics on Coordinators
  //////////////////////////////////////////////////////////////////////////////
  using MapReduce = void (*)(Metrics& /*reduced metrics*/,
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
                                MetricValue const&, bool /*ensure whitespace*/);

  //////////////////////////////////////////////////////////////////////////////
  /// Registration of some metric. We need to pass it name, and callbacks.
  ///
  /// @param mapReduce used to accumulate metrics
  /// @param toPrometheus used to print accumulated metrics in Prometheus format
  //////////////////////////////////////////////////////////////////////////////
  void add(std::string_view metric, MapReduce mapReduce,
           ToPrometheus toPrometheus);

  //////////////////////////////////////////////////////////////////////////////
  /// @see add but sometimes we want to accumulate some metrics from DBServers,
  /// but we don't want to make them public, so we don't need toPrometheus
  //////////////////////////////////////////////////////////////////////////////
  void add(std::string_view metric, MapReduce mapReduce);

  void toPrometheus(std::string& result, std::string_view globals,
                    bool ensureWhitespace) const;

  std::shared_ptr<Data> getData() const;

 private:
  mutable std::shared_mutex _m;
  containers::FlatHashMap<std::string_view, MapReduce> _mapReduce;
  containers::FlatHashMap<std::string_view, ToPrometheus> _toPrometheus;

  void rescheduleTimer(uint32_t timeoutMs) noexcept;
  void rescheduleUpdate(uint32_t timeoutMs) noexcept;

  void update();
  void repeatUpdate(uint32_t timeoutMs) noexcept;
  bool writeData(uint64_t version, futures::Try<RawDBServers>&& raw);
  bool readData(futures::Try<LeaderResponse>&& raw);
  Metrics parse(RawDBServers&& metrics) const;

  bool wasStop() const noexcept;

  // We don't want to update constantly empty data
  // It should be atomic only because write_global could cause parallel update
  std::atomic_bool _prevEmpty{true};
  std::shared_ptr<Data> _data;
  Scheduler::WorkHandle _update;
  Scheduler::WorkHandle _timer;
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  uint32_t _timeout = 10;
#else
  uint32_t _timeout = 0;
#endif

  static constexpr uint32_t kStop = 1;
  static constexpr uint32_t kUpdate = 2;
  std::atomic_uint32_t _count = 0;
};

}  // namespace arangodb::metrics
