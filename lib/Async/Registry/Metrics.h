#pragma once

#include "Metrics/Fwd.h"

#include <cstdint>
#include <memory>

namespace arangodb::metrics {
class MetricsFeature;
}

namespace arangodb::async_registry {

struct Metrics {
  // Metrics(arangodb::metrics::MetricsFeature& metrics_feature);
  Metrics() = default;
  Metrics(std::shared_ptr<metrics::Gauge<std::uint64_t>> running_coroutines,
          std::shared_ptr<metrics::Gauge<std::uint64_t>>
              coroutines_ready_for_deletion,
          std::shared_ptr<metrics::Gauge<std::uint64_t>>
              coroutine_thread_registries)
      : running_coroutines{running_coroutines},
        coroutines_ready_for_deletion{coroutines_ready_for_deletion},
        coroutine_thread_registries{coroutine_thread_registries} {}

  std::shared_ptr<metrics::Gauge<std::uint64_t>> running_coroutines = nullptr;
  std::shared_ptr<metrics::Gauge<std::uint64_t>> coroutines_ready_for_deletion =
      nullptr;
  std::shared_ptr<metrics::Gauge<std::uint64_t>> coroutine_thread_registries =
      nullptr;
};

}  // namespace arangodb::async_registry
