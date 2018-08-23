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

#ifndef ARANGOD_AQL_COST_ESTIMATION_H
#define ARANGOD_AQL_COST_ESTIMATION_H 1

#include "Basics/Common.h"

namespace arangodb {
namespace aql {

/// @brief a simple object containing a cost estimation for ExecutionNodes
struct CostEstimation {
  CostEstimation() : 
    estimatedCost(-1.0),
    estimatedNrItems(0) {}

  void reset() {
    // a value of < 0 will mean that the cost estimation was not performed yet
    estimatedCost = -1.0;
    estimatedNrItems = 0;
  }
  
  bool isSet() const {
    // a value of < 0 will mean that the cost estimation was not performed yet
    return estimatedCost >= 0.0;
  }

  double estimatedCost;
  size_t estimatedNrItems;
};

}
}

#endif
