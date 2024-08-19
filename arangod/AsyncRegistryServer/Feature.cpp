#include "Feature.h"

#include "Metrics/GaugeBuilder.h"
#include "Metrics/MetricsFeature.h"

using namespace arangodb::async_registry;

DECLARE_GAUGE(arangodb_coroutine_running_coroutines, std::uint64_t,
              "Number of currently running coroutines on the head");
DECLARE_GAUGE(
    arangodb_coroutine_coroutines_ready_for_deletion, std::uint64_t,
    "Number of finished coroutines that wait for their garbage collection");
DECLARE_GAUGE(arangodb_coroutine_coroutine_thread_registries, std::uint64_t,
              "Number of coroutine thread registries");

auto Feature::create_metrics(arangodb::metrics::MetricsFeature& metrics_feature)
    -> std::shared_ptr<const Metrics> {
  return std::make_shared<Metrics>(
      metrics_feature.addShared(arangodb_coroutine_running_coroutines{}),
      metrics_feature.addShared(
          arangodb_coroutine_coroutines_ready_for_deletion{}),
      metrics_feature.addShared(
          arangodb_coroutine_coroutine_thread_registries{}));
}
