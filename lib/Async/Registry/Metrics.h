#pragma once

#include "Metrics/Fwd.h"

#include <cstdint>
#include <memory>

namespace arangodb::metrics {
class MetricsFeature;
}

namespace arangodb::async_registry {

struct Metrics {
  Metrics() = default;
  Metrics(std::shared_ptr<metrics::Gauge<std::uint64_t>> active_functions,
          std::shared_ptr<metrics::Gauge<std::uint64_t>>
              ready_for_deletion_functions,
          std::shared_ptr<metrics::Gauge<std::uint64_t>> active_threads,
          std::shared_ptr<metrics::Gauge<std::uint64_t>> registered_threads,
          std::shared_ptr<metrics::Counter> total_threads,
          std::shared_ptr<metrics::Counter> total_functions)
      : active_functions{active_functions},
        ready_for_deletion_functions{ready_for_deletion_functions},
        running_threads{active_threads},
        registered_threads{registered_threads},
        total_threads{total_threads},
        total_functions{total_functions} {}

  std::shared_ptr<metrics::Gauge<std::uint64_t>> active_functions = nullptr;
  std::shared_ptr<metrics::Gauge<std::uint64_t>> ready_for_deletion_functions =
      nullptr;
  std::shared_ptr<metrics::Gauge<std::uint64_t>> running_threads = nullptr;
  std::shared_ptr<metrics::Gauge<std::uint64_t>> registered_threads = nullptr;
  std::shared_ptr<metrics::Counter> total_threads = nullptr;
  std::shared_ptr<metrics::Counter> total_functions = nullptr;
};

}  // namespace arangodb::async_registry
