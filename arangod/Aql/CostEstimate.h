////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
#include <stack>
#include <vector>

namespace arangodb::aql {

/// @brief a simple object containing a cost estimate for ExecutionNodes
struct CostEstimate {
  /// @brief create the cost estimate from values
  CostEstimate(double estimatedCost, std::size_t estimatedNrItems);

  /// @brief initialize a still-invalid cost estimate
  CostEstimate();

  [[nodiscard]] bool operator==(CostEstimate const& other) const;

  /// @brief return an empty but valid cost estimate
  [[nodiscard]] static CostEstimate empty();

  /// @brief invalidate the cost estimate
  void invalidate();

  /// @brief invalidate to a valid but empty estimate
  void initialize();

  /// @brief whether or not the estimate is valid
  [[nodiscard]] bool isValid() const;

  /// @brief estimated cost produced by the node, including cost of upstream nodes.
  double estimatedCost;

  /// @brief estimated number of items returned by the node
  std::size_t estimatedNrItems;

  /// @brief Push the current estimatedNrItems on top of an internal stack.
  ///        estimatedNrItems itself is unchanged.
  ///        This is used on SubqueryStartNodes to save the number of items for
  ///        the corresponding SubqueryEndNode to continue with later.
  void saveEstimatedNrItems();

  /// @brief Set estimatedNrItems to the top of the internal stack, and pop
  ///        the top of stack.
  ///        Used on SubqueryEndNodes to restore the number of items available
  ///        at the corresponding SubqueryStartNode.
  void restoreEstimatedNrItems();

 private:
  std::stack<std::size_t, std::vector<std::size_t>> _outerSubqueryEstimatedNrItems;
};

}  // namespace arangodb::aql

#endif
