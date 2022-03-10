////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#include "Metrics/MetricKey.h"
#include "Basics/debugging.h"

#include <string>
#include <tuple>

namespace arangodb::metrics {

MetricKey::MetricKey() noexcept = default;

MetricKey::MetricKey(std::string_view name) noexcept : name{name} {
  // the metric name should not include any spaces
  TRI_ASSERT(name.find(' ') == std::string::npos);
}

MetricKey::MetricKey(std::string_view name, std::string_view labels) noexcept
    : name{name}, labels{labels} {
  // the metric name should not include any spaces
  TRI_ASSERT(name.find(' ') == std::string::npos);
}

bool operator<(MetricKey const& lhs, MetricKey const& rhs) {
  return std::tie(lhs.name, lhs.labels) < std::tie(rhs.name, rhs.labels);
}

}  // namespace arangodb::metrics
