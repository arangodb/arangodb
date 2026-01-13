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

#include "Containers/Concurrent/metrics.h"
#include "Metrics/Fwd.h"

namespace arangodb::task_monitoring {

struct RegistryMetrics : arangodb::containers::Metrics {
  RegistryMetrics(
      std::shared_ptr<arangodb::metrics::Counter> tasks_total,
      std::shared_ptr<arangodb::metrics::Gauge<std::uint64_t>> existing_tasks,
      std::shared_ptr<arangodb::metrics::Gauge<std::uint64_t>>
          ready_for_deletion_tasks,
      std::shared_ptr<arangodb::metrics::Counter> thread_registries_total,
      std::shared_ptr<arangodb::metrics::Gauge<std::uint64_t>>
          existing_thread_registries)
      : tasks_total{tasks_total},
        existing_tasks{existing_tasks},
        ready_for_deletion_tasks{ready_for_deletion_tasks},
        thread_registries_total{thread_registries_total},
        existing_thread_registries{existing_thread_registries} {}
  ~RegistryMetrics() = default;
  auto increment_total_nodes() -> void override;
  auto increment_registered_nodes() -> void override;
  auto decrement_registered_nodes() -> void override;
  auto increment_ready_for_deletion_nodes() -> void override;
  auto decrement_ready_for_deletion_nodes() -> void override;
  auto increment_total_lists() -> void override;
  auto increment_existing_lists() -> void override;
  auto decrement_existing_lists() -> void override;

 private:
  std::shared_ptr<arangodb::metrics::Counter> tasks_total = nullptr;
  std::shared_ptr<arangodb::metrics::Gauge<std::uint64_t>> existing_tasks =
      nullptr;
  std::shared_ptr<arangodb::metrics::Gauge<std::uint64_t>>
      ready_for_deletion_tasks = nullptr;
  std::shared_ptr<arangodb::metrics::Counter> thread_registries_total = nullptr;
  std::shared_ptr<arangodb::metrics::Gauge<std::uint64_t>>
      existing_thread_registries = nullptr;
};

}  // namespace arangodb::task_monitoring
