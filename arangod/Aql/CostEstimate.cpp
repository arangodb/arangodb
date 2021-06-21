////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "CostEstimate.h"

#include "Basics/debugging.h"

using namespace arangodb;
using namespace arangodb::aql;
CostEstimate::CostEstimate(double estimatedCost, std::size_t estimatedNrItems)
    : estimatedCost(estimatedCost), estimatedNrItems(estimatedNrItems) {}

CostEstimate::CostEstimate() : CostEstimate(-1.0, 0) {}

bool CostEstimate::operator==(CostEstimate const& other) const {
  return estimatedCost == other.estimatedCost && estimatedNrItems == other.estimatedNrItems;
}

CostEstimate CostEstimate::empty() { return {0.0, 0}; }

void CostEstimate::invalidate() {
  // a value of < 0 will mean that the cost estimation was not performed yet
  estimatedCost = -1.0;
  estimatedNrItems = 0;
  TRI_ASSERT(!isValid());
}

void CostEstimate::initialize() {
  estimatedCost = 0.0;
  estimatedNrItems = 0;
  TRI_ASSERT(isValid());
}

bool CostEstimate::isValid() const {
  // a value of < 0 will mean that the cost estimation was not performed yet
  return estimatedCost >= 0.0;
}

void CostEstimate::saveEstimatedNrItems() {
  _outerSubqueryEstimatedNrItems.push(estimatedNrItems);
}

void CostEstimate::restoreEstimatedNrItems() {
  TRI_ASSERT(!_outerSubqueryEstimatedNrItems.empty());
  estimatedNrItems = _outerSubqueryEstimatedNrItems.top();
  _outerSubqueryEstimatedNrItems.pop();
}
