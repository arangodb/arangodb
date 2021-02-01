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
/// @author Heiko Kernbach
/// @author Lars Maier
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_PREGEL_ALGOS_ACCUMULATORS_VERTEXCOMPUTATION_H
#define ARANGODB_PREGEL_ALGOS_ACCUMULATORS_VERTEXCOMPUTATION_H 1

#include "AIR.h"

#include "Greenspun/Interpreter.h"

namespace arangodb {
namespace pregel {
namespace algos {
namespace accumulators {

struct WorkerContext;

class VertexComputation : public vertex_computation {
 public:
  explicit VertexComputation(ProgrammablePregelAlgorithm const& algorithm);
  VertexComputation(VertexComputation&&) = delete;
  VertexComputation& operator=(VertexComputation&&) = delete;

  void compute(MessageIterator<message_type> const& messages) override;
  ProgrammablePregelAlgorithm const& algorithm() const;
  WorkerContext const& workerContext() const;

  static greenspun::EvalResult air_accumRef_helper(VPackSlice const params,
                                                   VPackBuilder& result,
                                                   vertex_type const*);

 private:
  greenspun::EvalResult clearAllVertexAccumulators();
  greenspun::EvalResultT<bool> processIncomingMessages(MessageIterator<MessageData> const& incomingMessages);
  void traceMessage(MessageData const*);

  greenspun::EvalResult runProgram(greenspun::Machine& ctx, VPackSlice program);

  void registerLocalFunctions();

  // Local functions
  greenspun::EvalResult air_accumRef(greenspun::Machine& ctx,
                                     VPackSlice const params, VPackBuilder& result);
  greenspun::EvalResult air_accumSet(greenspun::Machine& ctx,
                                     VPackSlice const params, VPackBuilder& result);
  greenspun::EvalResult air_accumClear(greenspun::Machine& ctx,
                                       VPackSlice const params, VPackBuilder& result);

  greenspun::EvalResult air_sendToAccum(greenspun::Machine& ctx,
                                        VPackSlice const params, VPackBuilder& result);
  greenspun::EvalResult air_sendToAllNeighbors(greenspun::Machine& ctx, VPackSlice const params,
                                               VPackBuilder& result);

  greenspun::EvalResult air_globalAccumRef(greenspun::Machine& ctx, VPackSlice const params,
                                           VPackBuilder& result);
  greenspun::EvalResult air_sendToGlobalAccum(greenspun::Machine& ctx, VPackSlice const params,
                                              VPackBuilder& result);

  greenspun::EvalResult air_outboundEdges(greenspun::Machine& ctx,
                                          VPackSlice const params, VPackBuilder& result);
  greenspun::EvalResult air_numberOutboundEdges(greenspun::Machine& ctx,
                                                VPackSlice const params,
                                                VPackBuilder& result);
  greenspun::EvalResult air_numberOfVertices(greenspun::Machine& ctx, VPackSlice const params,
                                             VPackBuilder& result);
  greenspun::EvalResult air_bindRef(greenspun::Machine& ctx,
                                    VPackSlice const params, VPackBuilder& result);

  greenspun::EvalResult air_thisDoc(greenspun::Machine& ctx,
                                    VPackSlice const params, VPackBuilder& result);
  greenspun::EvalResult air_thisVertexId(greenspun::Machine& ctx,
                                         VPackSlice const params, VPackBuilder& result);
  greenspun::EvalResult air_thisUniqueId(greenspun::Machine& ctx,
                                         VPackSlice const params, VPackBuilder& result);
  greenspun::EvalResult air_thisPregelId(greenspun::Machine& ctx,
                                         VPackSlice const params, VPackBuilder& result);

  greenspun::EvalResult air_globalSuperstep(greenspun::Machine& ctx, VPackSlice const params,
                                            VPackBuilder& result);

 private:
  ProgrammablePregelAlgorithm const& _algorithm;
  greenspun::Machine _airMachine;
};

}  // namespace accumulators
}  // namespace algos
}  // namespace pregel
}  // namespace arangodb
#endif
