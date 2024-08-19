#pragma once

#include "Metrics/Fwd.h"

#include <cstdint>
#include <memory>

namespace arangodb::async_registry {

struct Metrics {
  std::shared_ptr<metrics::Gauge<std::uint64_t>> running_coroutines;
  std::shared_ptr<metrics::Gauge<std::uint64_t>> coroutines_ready_for_deletion;
  std::shared_ptr<metrics::Gauge<std::uint64_t>> coroutine_thread_registries;
};

}  // namespace arangodb::async_registry
