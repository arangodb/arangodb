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
              ready_for_deletion_coroutines,
          std::shared_ptr<metrics::Gauge<std::uint64_t>> running_threads,
          std::shared_ptr<metrics::Gauge<std::uint64_t>> registered_threads,
          std::shared_ptr<metrics::Counter> total_threads,
          std::shared_ptr<metrics::Counter> total_coroutines)
      : running_coroutines{running_coroutines},
        ready_for_deletion_coroutines{ready_for_deletion_coroutines},
        running_threads{running_threads},
        registered_threads{registered_threads},
        total_threads{total_threads},
        total_coroutines{total_coroutines} {}

  std::shared_ptr<metrics::Gauge<std::uint64_t>> running_coroutines = nullptr;
  std::shared_ptr<metrics::Gauge<std::uint64_t>> ready_for_deletion_coroutines =
      nullptr;
  std::shared_ptr<metrics::Gauge<std::uint64_t>> running_threads = nullptr;
  std::shared_ptr<metrics::Gauge<std::uint64_t>> registered_threads = nullptr;
  std::shared_ptr<metrics::Counter> total_threads = nullptr;
  std::shared_ptr<metrics::Counter> total_coroutines = nullptr;
};

}  // namespace arangodb::async_registry
