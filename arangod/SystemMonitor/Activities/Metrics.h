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

#include "Activities/IRegistryMetrics.h"
#include "Metrics/Fwd.h"

#include <memory>

namespace arangodb::activities {

struct RegistryMetrics : IRegistryMetrics {
  RegistryMetrics(std::shared_ptr<arangodb::metrics::Counter> activities_total,
                  std::shared_ptr<arangodb::metrics::Gauge<std::uint64_t>>
                      existing_activities)

      : activities_total{activities_total},
        existing_activities{existing_activities} {}
  ~RegistryMetrics() = default;
  auto increment_total_nodes() -> void override;
  auto increment_registered_nodes() -> void override;
  auto store_registered_nodes(std::uint64_t count) -> void override;

 private:
  std::shared_ptr<arangodb::metrics::Counter> activities_total = nullptr;
  std::shared_ptr<arangodb::metrics::Gauge<std::uint64_t>> existing_activities =
      nullptr;
};

}  // namespace arangodb::activities
