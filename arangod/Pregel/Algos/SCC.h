////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_PREGEL_ALGOS_SSSP_H
#define ARANGODB_PREGEL_ALGOS_SSSP_H 1

#include "Pregel/Algorithm.h"

namespace arangodb {
namespace pregel {
namespace algos {

/// Strongly connected components.
struct SCCAlgorithm : public Algorithm<int64_t, int64_t, int64_t> {
 public:
  SCCAlgorithm() : Algorithm("SCC") {}

  std::shared_ptr<GraphFormat<int64_t, int64_t>> inputFormat() const override;
  std::shared_ptr<MessageFormat<int64_t>> messageFormat() const override;
  std::shared_ptr<MessageCombiner<int64_t>> messageCombiner() const override;
  std::shared_ptr<VertexComputation<int64_t, int64_t, int64_t>>
  createComputation() const override;
};
}
}
}
#endif
