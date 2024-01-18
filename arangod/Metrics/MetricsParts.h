////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
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

#include <cstdint>
#include <type_traits>

namespace arangodb::metrics {

// metrics "section".
enum class MetricsSection : std::uint8_t {
  None = 0,

  // note that all "real" values must be mutually exclusive powers of 2
  Standard = 1,
  Dynamic = 2,

  All = Standard | Dynamic,
};

using MetricsSectionType = std::underlying_type<MetricsSection>::type;

// what types of metrics to include in a response.
struct MetricsParts {
  MetricsParts() noexcept
      : sections(static_cast<MetricsSectionType>(MetricsSection::None)) {}
  explicit MetricsParts(MetricsSection section) noexcept
      : sections(static_cast<MetricsSectionType>(section)) {}

  void add(MetricsSection section) noexcept {
    sections |= static_cast<MetricsSectionType>(section);
  }

  bool includeStandardMetrics() const noexcept {
    return sections & static_cast<MetricsSectionType>(MetricsSection::Standard);
  }

  bool includeDynamicMetrics() const noexcept {
    return sections & static_cast<MetricsSectionType>(MetricsSection::Dynamic);
  }

  MetricsSectionType sections;
};

}  // namespace arangodb::metrics
