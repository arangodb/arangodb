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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/LazyApplicationFeatureReference.h"
#include "Basics/DownCast.h"
#include "Containers/FlatHashMap.h"
#include "Metrics/Batch.h"
#include "Metrics/Builder.h"
#include "Metrics/CollectMode.h"
#include "Metrics/IBatch.h"
#include "Metrics/IRegistry.h"
#include "Metrics/Metric.h"
#include "Metrics/MetricKey.h"
#include "Metrics/MetricsOptions.h"
#include "Metrics/MetricsParts.h"

#include <map>
#include <shared_mutex>

namespace arangodb {
class QueryRegistryFeature;
class DatabaseFeature;
class ClusterFeature;
}  // namespace arangodb
namespace arangodb::metrics {

class ClusterMetricsFeature;

class MetricsFeature final : public application_features::ApplicationFeature,
                             public IRegistry {
 public:
  // Maintain backward compatibility for existing code
  using UsageTrackingMode = metrics::UsageTrackingMode;

  static constexpr std::string_view name() noexcept { return "Metrics"; }

  MetricsFeature(
      application_features::ApplicationServer& server,
      LazyApplicationFeatureReference<QueryRegistryFeature>
          lazyQueryRegistryFeatureRef,
      LazyApplicationFeatureReference<DatabaseFeature> lazyDatabaseFeatureRef,
      LazyApplicationFeatureReference<ClusterMetricsFeature>
          lazyClusterMetricsFeatureRef,
      LazyApplicationFeatureReference<ClusterFeature> lazyClusterFeatureRef);

  MetricsFeature(
      application_features::ApplicationServer& server,
      LazyApplicationFeatureReference<QueryRegistryFeature>
          lazyQueryRegistryFeatureRef,
      LazyApplicationFeatureReference<DatabaseFeature> lazyDatabaseFeatureRef,
      LazyApplicationFeatureReference<ClusterMetricsFeature>
          lazyClusterMetricsFeatureRef,
      LazyApplicationFeatureReference<ClusterFeature> lazyClusterFeatureRef,
      MetricsOptions options);

  bool exportAPI() const noexcept;
  bool ensureWhitespace() const noexcept;
  UsageTrackingMode usageTrackingMode() const noexcept;

  // tries to add the metric. If the metric already exists, it is returned
  // instead.
  template<typename MetricBuilder>
  auto ensureMetric(MetricBuilder&& builder) ->
      typename MetricBuilder::MetricT& {
    return static_cast<typename MetricBuilder::MetricT&>(
        *doEnsureMetric(builder));
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

  static double serverUptime() noexcept;

 protected:
  std::shared_ptr<Metric> doAdd(Builder& builder) override;

 private:
  std::shared_ptr<Metric> doAddDynamic(Builder& builder);
  std::shared_ptr<Metric> doEnsureMetric(Builder& builder);
  std::shared_lock<std::shared_mutex> initGlobalLabels() const;

  LazyApplicationFeatureReference<QueryRegistryFeature>
      _lazyQueryRegistryFeatureRef;
  LazyApplicationFeatureReference<DatabaseFeature> _lazyDatabaseFeatureRef;
  LazyApplicationFeatureReference<ClusterMetricsFeature>
      _lazyClusterMetricsFeatureRef;
  LazyApplicationFeatureReference<ClusterFeature> _lazyClusterFeatureRef;

  QueryRegistryFeature* _queryRegistryFeature = nullptr;
  DatabaseFeature* _databaseFeature = nullptr;
  ClusterMetricsFeature* _clusterMetricsFeature = nullptr;
  ClusterFeature* _clusterFeature = nullptr;

  mutable std::shared_mutex _mutex;

  // TODO(MBkkt) abseil btree map? or hashmap<name, hashmap<labels, Metric>>?
  std::map<MetricKeyView, std::shared_ptr<Metric>> _registry;

  containers::FlatHashMap<std::string_view, std::unique_ptr<IBatch>> _batch;

  mutable std::string _globals;
  mutable bool hasShortname = false;
  mutable bool hasRole = false;

  MetricsOptions _options;

  static double _serverStartTime;
};

}  // namespace arangodb::metrics
