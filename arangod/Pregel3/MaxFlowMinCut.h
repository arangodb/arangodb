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
/// @author Roman Rabinovich
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vector>
#include "Graph.h"
#include "Basics/ResultT.h"

namespace arangodb::pregel3 {

typedef std::vector<std::pair<size_t, size_t>> Flow;
typedef std::vector<std::pair<size_t, size_t>> Cut;

struct MaxFlowMinCutResult {
  Flow flow;
  Cut cut;
};

template<class GraphProps, class VertexProps, class EdgeProps>
class MaxFlowMinCut {
  /**
   * If the input is correct, compute a maximum flow and the corresponding
   * mincut. Otherwise return TRI_ERROR_BAD_PARAMETER.
   * @param g
   * @param sourceIdx
   * @param targetIdx
   * @return
   */
  ResultT<MaxFlowMinCutResult> run(BaseGraph* g, size_t sourceIdx,
                                   size_t targetIdx);
};

}  // namespace arangodb::pregel3