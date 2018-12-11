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

#include "Basics/Common.h"

namespace arangodb {
namespace aql {

/// @brief a simple object containing a cost estimate for ExecutionNodes
struct CostEstimate {
  /// @brief create the cost estimate from values
  CostEstimate(double estimatedCost, size_t estimatedNrItems)
      : estimatedCost(estimatedCost),
        estimatedNrItems(estimatedNrItems) {} 
  
  /// @brief initialize a still-invalid cost estimate
  CostEstimate() : CostEstimate(-1.0, 0) {}

  bool operator==(CostEstimate const& other) const {
    return estimatedCost == other.estimatedCost && estimatedNrItems == other.estimatedNrItems;
  }

  /// @brief return an empty but valid cost estimate
  static CostEstimate empty() { return CostEstimate(0.0, 0); }

  /// @brief invalidate the cost estimate
  void invalidate() {
    // a value of < 0 will mean that the cost estimation was not performed yet
    estimatedCost = -1.0;
    estimatedNrItems = 0;
    TRI_ASSERT(!isValid());
  }
  
  /// @brief invalidate to a valid but empty estimate
  void initialize() {
    estimatedCost = 0.0;
    estimatedNrItems = 0;
    TRI_ASSERT(isValid());
  }
  
  /// @brief whether or not the estimate is valid
  inline bool isValid() const {
    // a value of < 0 will mean that the cost estimation was not performed yet
    return estimatedCost >= 0.0;
  }

  /// @brief estimated cost produced by the node
  double estimatedCost;
  
  /// @brief cost
  /// @brief estimated number of items returned by the node
  size_t estimatedNrItems;
};

}
}

#endif
