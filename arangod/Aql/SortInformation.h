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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string>
#include <tuple>
#include <vector>

namespace arangodb::aql {
class ExecutionNode;

/// @brief this is an auxilliary struct for processed sort criteria information
struct SortInformation {
  enum Match {
    unequal,                // criteria are unequal
    otherLessAccurate,      // leftmost sort criteria are equal, but other sort
                            // criteria are less accurate than ourselves
    ourselvesLessAccurate,  // leftmost sort criteria are equal, but our own
                            // sort criteria is less accurate than the other
    allEqual                // all criteria are equal
  };

  std::vector<std::tuple<ExecutionNode const*, std::string, bool>> criteria;
  bool isValid = true;
  bool isDeterministic = true;
  bool isComplex = false;

  Match isCoveredBy(SortInformation const& other);
};

}  // namespace arangodb::aql
