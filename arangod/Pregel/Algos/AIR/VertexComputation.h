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

#ifndef ARANGODB_PREGEL_ALGOS_ACCUMULATORS_VERTEXCOMPUTATION_H
#define ARANGODB_PREGEL_ALGOS_ACCUMULATORS_VERTEXCOMPUTATION_H 1

#include "AIR.h"

#include "Greenspun/Interpreter.h"

namespace arangodb {
namespace pregel {
namespace algos {
namespace accumulators {

class VertexComputation : public vertex_computation {
 public:
  explicit VertexComputation(VertexAccumulators const& algorithm);
  void compute(MessageIterator<message_type> const& messages) override;
  VertexAccumulators const& algorithm() const;

 private:
  bool processIncomingMessages(MessageIterator<MessageData> const& incomingMessages);
  void runProgram(greenspun::Machine& ctx, VPackSlice program);

  void registerLocalFunctions();

  // Local functions
  greenspun::EvalResult air_accumRef(greenspun::Machine& ctx, VPackSlice const params, VPackBuilder& result);
  greenspun::EvalResult air_accumSet(greenspun::Machine& ctx, VPackSlice const params, VPackBuilder& result);
  greenspun::EvalResult air_accumClear(greenspun::Machine& ctx, VPackSlice const params, VPackBuilder& result);

  greenspun::EvalResult air_sendToAccum(greenspun::Machine& ctx, VPackSlice const params, VPackBuilder& result);
  greenspun::EvalResult air_sendToAllNeighbours(greenspun::Machine& ctx, VPackSlice const params, VPackBuilder& result);

  greenspun::EvalResult air_outboundEdges(greenspun::Machine& ctx, VPackSlice const params, VPackBuilder& result);
  greenspun::EvalResult air_neigbours(greenspun::Machine& ctx, VPackSlice const params, VPackBuilder& result);
  greenspun::EvalResult air_numberOutboundEdges(greenspun::Machine& ctx, VPackSlice const params, VPackBuilder& result);
  greenspun::EvalResult air_numberOfVertices(greenspun::Machine& ctx, VPackSlice const params, VPackBuilder& result);

 private:
  VertexAccumulators const& _algorithm;
  greenspun::Machine _airMachine;
};

}  // namespace accumulators
}  // namespace algos
}  // namespace pregel
}  // namespace arangodb
#endif
