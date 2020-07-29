////////////////////////////////////////////////////////////////////////////////
///
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Heiko Kernbach
/// @author Lars Maier
/// @author Markus Pfeiffer
///
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_PREGEL_ALGOS_VERTEX_COMPUTATION_EVALCONTEXT_H
#define ARANGODB_PREGEL_ALGOS_VERTEX_COMPUTATION_EVALCONTEXT_H 1

#include <Pregel/Algos/VertexAccumulators/Greenspun/Primitives.h>

#include "VertexAccumulators.h"

namespace arangodb {
namespace pregel {
namespace algos {
namespace accumulators {

class VertexComputation;
class VertexData;

class VertexComputationEvalContext : public PrimEvalContext {
 public:
  explicit VertexComputationEvalContext(VertexComputation& computation);

  /* This is the _id of the vertex document */
  std::string const& getThisId() const override;

  /* What is this used for? */
  std::size_t getVertexUniqueId() const override;

  /* what's done when print is called */
  void printCallback(const std::string& msg) const override;
  void getAccumulatorValue(std::string_view accumId, VPackBuilder& builder) const override;
  void setAccumulator(std::string_view accumId, VPackSlice value) override;

  EvalResult getPregelId(VPackBuilder& result) const override;
  void updateAccumulator(std::string_view accumId, std::string_view toId,
                         VPackSlice value) override;
  EvalResult updateAccumulatorById(std::string_view accumId, VPackSlice toVertex,
                                   VPackSlice value) override;
  EvalResult enumerateEdges(std::function<EvalResult(VPackSlice edge)> cb) const override;
  EvalResult getBindingValue(std::string_view id, VPackBuilder& result) const override;
  EvalResult getGlobalSuperstep(VPackBuilder& result) const override;

 private:

  VertexComputation& computation() const;
  VertexData& vertexData() const;

  VertexComputation& _computation;
};

}  // namespace accumulators
}  // namespace algos
}  // namespace pregel
}  // namespace arangodb

#endif
