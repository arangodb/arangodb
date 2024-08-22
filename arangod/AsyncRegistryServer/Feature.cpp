#include "Feature.h"

#include "Metrics/CounterBuilder.h"
#include "Metrics/GaugeBuilder.h"
#include "Metrics/MetricsFeature.h"

using namespace arangodb::async_registry;

DECLARE_GAUGE(arangodb_async_running_coroutines, std::uint64_t,
              "Number of currently running coroutines on the head");
DECLARE_GAUGE(
    arangodb_async_ready_for_deletion_coroutines, std::uint64_t,
    "Number of finished coroutines that wait for their garbage collection");
DECLARE_GAUGE(arangodb_async_running_threads, std::uint64_t,
              "Number of active coroutine threads");
DECLARE_GAUGE(arangodb_async_registered_threads, std::uint64_t,
              "Number of registered coroutine threads");
DECLARE_COUNTER(arangodb_async_total_threads,
                "Total number of created coroutine threads");
DECLARE_COUNTER(arangodb_async_total_coroutines,
                "Total number of created coroutines");

auto Feature::create_metrics(arangodb::metrics::MetricsFeature& metrics_feature)
    -> std::shared_ptr<const Metrics> {
  return std::make_shared<Metrics>(
      metrics_feature.addShared(arangodb_async_running_coroutines{}),
      metrics_feature.addShared(arangodb_async_ready_for_deletion_coroutines{}),
      metrics_feature.addShared(arangodb_async_running_threads{}),
      metrics_feature.addShared(arangodb_async_registered_threads{}),
      metrics_feature.addShared(arangodb_async_total_threads{}),
      metrics_feature.addShared(arangodb_async_total_coroutines{}));
}
