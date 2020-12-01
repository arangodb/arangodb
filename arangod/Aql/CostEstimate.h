////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_AQL_COST_ESTIMATE_H
#define ARANGOD_AQL_COST_ESTIMATE_H 1

#include <cstddef>

namespace arangodb {
namespace aql {

/// @brief a simple object containing a cost estimate for ExecutionNodes
struct CostEstimate {
  /// @brief create the cost estimate from values
  CostEstimate(double estimatedCost, std::size_t estimatedNrItems);

  /// @brief initialize a still-invalid cost estimate
  CostEstimate();

  bool operator==(CostEstimate const& other) const;

  /// @brief return an empty but valid cost estimate
  static CostEstimate empty();

  /// @brief invalidate the cost estimate
  void invalidate();

  /// @brief invalidate to a valid but empty estimate
  void initialize();

  /// @brief whether or not the estimate is valid
  bool isValid() const;

  /// @brief estimated cost produced by the node
  double estimatedCost;

  /// @brief cost
  /// @brief estimated number of items returned by the node
  std::size_t estimatedNrItems;
};

}  // namespace aql
}  // namespace arangodb

#endif
