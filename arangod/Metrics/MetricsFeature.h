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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ApplicationFeatures/LazyApplicationFeatureReference.h"
#include "Basics/DownCast.h"
#include "Containers/FlatHashMap.h"
#include "Metrics/Batch.h"
#include "Metrics/Builder.h"
#include "Metrics/CollectMode.h"
#include "Metrics/IBatch.h"
#include "Metrics/Metric.h"
#include "Metrics/MetricKey.h"
#include "Metrics/MetricsParts.h"
#include "ProgramOptions/ProgramOptions.h"
#include "RestServer/arangod.h"
#include "Statistics/ServerStatistics.h"

#include <map>
#include <shared_mutex>

namespace arangodb::metrics {

class MetricsFeature final : public ApplicationFeature {
 public:
  enum class UsageTrackingMode {
    // no tracking
    kDisabled,
    // tracking per shard (one-dimensional)
    kEnabledPerShard,
    // tracking per shard and per user (two-dimensional)
    kEnabledPerShardPerUser,
  };

  static constexpr std::string_view name() noexcept { return "Metrics"; }

  template<typename Server>
  explicit MetricsFeature(
      Server& server,
      LazyApplicationFeatureReference<QueryRegistryFeature>
          lazyQueryRegistryFeatureRef,
      LazyApplicationFeatureReference<StatisticsFeature>
          lazyStatisticsFeatureRef,
      LazyApplicationFeatureReference<EngineSelectorFeature>
          lazyEngineSelectorFeatureRef,
      LazyApplicationFeatureReference<ClusterMetricsFeature>
          lazyClusterMetricsFeatureRef,
      LazyApplicationFeatureReference<ClusterFeature> lazyClusterFeatureRef);

  bool exportAPI() const noexcept;
  bool ensureWhitespace() const noexcept;
  UsageTrackingMode usageTrackingMode() const noexcept;

  void collectOptions(std::shared_ptr<options::ProgramOptions>) final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) final;

  // tries to add metric. throws if such metric already exists
  template<typename MetricBuilder>
  auto add(MetricBuilder&& builder) -> typename MetricBuilder::MetricT& {
    return static_cast<typename MetricBuilder::MetricT&>(*doAdd(builder));
  }

  // tries to add the metric. If the metric already exists, it is returned
  // instead.
  template<typename MetricBuilder>
  auto ensureMetric(MetricBuilder&& builder) ->
      typename MetricBuilder::MetricT& {
    return static_cast<typename MetricBuilder::MetricT&>(
        *doEnsureMetric(builder));
  }

  template<typename MetricBuilder>
  auto addShared(MetricBuilder&& builder)  // TODO(MBkkt) Remove this method
      -> std::shared_ptr<typename MetricBuilder::MetricT> {
    return std::static_pointer_cast<typename MetricBuilder::MetricT>(
        doAdd(builder));
  }

  // tries to add dynamic metric. does not fail if such metric already exists
  template<typename MetricBuilder>
  auto addDynamic(MetricBuilder&& builder) -> typename MetricBuilder::MetricT& {
    return static_cast<typename MetricBuilder::MetricT&>(
        *doAddDynamic(builder));
  }

  Metric* get(MetricKeyView const& key) const;
  bool remove(Builder const& builder);
  bool remove(Metric const& m);

  void toPrometheus(std::string& result, MetricsParts metricsParts,
                    CollectMode mode) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief That used for collect some metrics
  /// to array for ClusterMetricsFeature
  //////////////////////////////////////////////////////////////////////////////
  void toVPack(velocypack::Builder& builder, MetricsParts metricsParts) const;

  ServerStatistics& serverStatistics() noexcept;

  template<typename MetricType>
  MetricType& batchAdd(std::string_view name, std::string_view labels) {
    std::unique_lock lock{_mutex};
    auto& iBatch = _batch[name];
    if (!iBatch) {
      iBatch = std::make_unique<metrics::Batch<MetricType>>();
    }
    return basics::downCast<metrics::Batch<MetricType>>(*iBatch).add(labels);
  }
  std::pair<std::shared_lock<std::shared_mutex>, metrics::IBatch*> getBatch(
      std::string_view name) const;
  void batchRemove(std::string_view name, std::string_view labels);

  void prepare() override;

 private:
  std::shared_ptr<Metric> doAdd(Builder& builder);
  std::shared_ptr<Metric> doAddDynamic(Builder& builder);
  std::shared_ptr<Metric> doEnsureMetric(Builder& builder);
  std::shared_lock<std::shared_mutex> initGlobalLabels() const;

  LazyApplicationFeatureReference<QueryRegistryFeature>
      _lazyQueryRegistryFeatureRef;
  LazyApplicationFeatureReference<StatisticsFeature> _lazyStatisticsFeatureRef;
  LazyApplicationFeatureReference<EngineSelectorFeature>
      _lazyEngineSelectorFeatureRef;
  LazyApplicationFeatureReference<ClusterMetricsFeature>
      _lazyClusterMetricsFeatureRef;
  LazyApplicationFeatureReference<ClusterFeature> _lazyClusterFeatureRef;

  QueryRegistryFeature* _queryRegistryFeature = nullptr;
  StatisticsFeature* _statisticsFeature = nullptr;
  EngineSelectorFeature* _engineSelectorFeature = nullptr;
  ClusterMetricsFeature* _clusterMetricsFeature = nullptr;
  ClusterFeature* _clusterFeature = nullptr;

  mutable std::shared_mutex _mutex;

  // TODO(MBkkt) abseil btree map? or hashmap<name, hashmap<labels, Metric>>?
  std::map<MetricKeyView, std::shared_ptr<Metric>> _registry;

  containers::FlatHashMap<std::string_view, std::unique_ptr<IBatch>> _batch;

  std::unique_ptr<ServerStatistics> _serverStatistics;

  mutable std::string _globals;
  mutable bool hasShortname = false;
  mutable bool hasRole = false;

  bool _export;
  bool _exportReadWriteMetrics;
  // ensure that there is whitespace before the reported value, regardless
  // of whether it is preceeded by labels or not.
  bool _ensureWhitespace;

  std::string _usageTrackingModeString;
  UsageTrackingMode _usageTrackingMode;
};

}  // namespace arangodb::metrics
