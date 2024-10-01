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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <string_view>

namespace arangodb::metrics {

template<typename T>
struct MetricKey {
  T name;
  T labels;
};

using MetricKeyView = MetricKey<std::string_view>;

template<typename L, typename R>
bool operator==(const MetricKey<L>& lhs, const MetricKey<R>& rhs) noexcept {
  return lhs.name == rhs.name && lhs.labels == rhs.labels;
}

template<typename L, typename R>
bool operator<(const MetricKey<L>& lhs, const MetricKey<R>& rhs) noexcept {
  const auto comp_name = lhs.name.compare(rhs.name);
  return comp_name < 0 || (comp_name == 0 && lhs.labels < rhs.labels);
}

}  // namespace arangodb::metrics
