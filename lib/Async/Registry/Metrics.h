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
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////
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
  Metrics(
      std::shared_ptr<metrics::Counter> promises_total,
      std::shared_ptr<metrics::Gauge<std::uint64_t>> existing_promises,
      std::shared_ptr<metrics::Gauge<std::uint64_t>>
          ready_for_deletion_promises,
      std::shared_ptr<metrics::Counter> thread_registries_total,
      std::shared_ptr<metrics::Gauge<std::uint64_t>> existing_thread_registries)
      : promises_total{promises_total},
        existing_promises{existing_promises},
        ready_for_deletion_promises{ready_for_deletion_promises},
        thread_registries_total{thread_registries_total},
        existing_thread_registries{existing_thread_registries} {}

  std::shared_ptr<metrics::Counter> promises_total = nullptr;
  std::shared_ptr<metrics::Gauge<std::uint64_t>> existing_promises = nullptr;
  std::shared_ptr<metrics::Gauge<std::uint64_t>> ready_for_deletion_promises =
      nullptr;
  std::shared_ptr<metrics::Counter> thread_registries_total = nullptr;
  std::shared_ptr<metrics::Gauge<std::uint64_t>> existing_thread_registries =
      nullptr;
};

}  // namespace arangodb::async_registry
