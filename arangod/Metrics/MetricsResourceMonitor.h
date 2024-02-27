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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Metrics/Gauge.h"
#include "Metrics/Metric.h"

#include <cstdint>

namespace arangodb::metrics {

template<typename Gauge>
class GaugeResourceMonitor {
  GaugeResourceMonitor(GaugeResourceMonitor const& other) = delete;
  GaugeResourceMonitor& operator=(GaugeResourceMonitor const& other) = delete;

 public:
  explicit GaugeResourceMonitor(Gauge& m) noexcept : _metric(m) {}

  /// @brief increase memory usage by <value> bytes. may throw!
  void increaseMemoryUsage(std::uint64_t value) { _metric.fetch_add(value); }

  /// @brief decrease memory usage by <value> bytes. will not throw
  void decreaseMemoryUsage(std::uint64_t value) noexcept {
    _metric.fetch_sub(value);
  }

 private:
  Gauge& _metric;
};

}  // namespace arangodb::metrics
