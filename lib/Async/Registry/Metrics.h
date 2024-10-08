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
