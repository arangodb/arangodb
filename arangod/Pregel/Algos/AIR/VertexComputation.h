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

struct WorkerContext;

class VertexComputation : public vertex_computation {
 public:
  explicit VertexComputation(VertexAccumulators const& algorithm);
  VertexComputation(VertexComputation&&) = delete;
  VertexComputation& operator=(VertexComputation&&) = delete;

  void compute(MessageIterator<message_type> const& messages) override;
  VertexAccumulators const& algorithm() const;
  WorkerContext const& workerContext() const;

 private:
  greenspun::EvalResult clearAllVertexAccumulators();
  greenspun::EvalResultT<bool> processIncomingMessages(MessageIterator<MessageData> const& incomingMessages);
  void traceMessage(MessageData const*);

  greenspun::EvalResult runProgram(greenspun::Machine& ctx, VPackSlice program);


  void registerLocalFunctions();

  // Local functions
  // TODO we might want to place these in a separate struct and call it
  //      the "vertex function library" or somesuch
  // TODO it has to be less cumbersome to declare these buggers. Maybe we implement
  //      these as structs with operator (), because the parameters are all the same
  //      and we can neatly wrap parameter checking inside?
  //
  // TODO Wrap and unwrap parameters using templates? Suppose I have a function I want
  //      to call from AIR that takes an uint64_t and a string and returns a bool,
  //      can we make template tricks to be able to declare
  //      blafunc<uint64_t, std::string, bool> which automatically unwraps
  //      into an appropriate call into the Machine, including error messages?
  //
  //      what about lists/variadic functions? might not be able to do them
  //
  //      register functions with type info & small documentation, so we can
  //      display it in the editor?

  greenspun::EvalResultT<std::unique_ptr<AccumulatorBase>&> vertexAccumulatorByName(std::string_view accumId);

  greenspun::EvalResult air_accumRef(greenspun::Machine& ctx,
                                     VPackSlice const params, VPackBuilder& result);
  greenspun::EvalResult air_accumSet(greenspun::Machine& ctx,
                                     VPackSlice const params, VPackBuilder& result);
  greenspun::EvalResult air_accumClear(greenspun::Machine& ctx,
                                       VPackSlice const params, VPackBuilder& result);



  greenspun::EvalResult air_sendToAccum(greenspun::Machine& ctx,
                                        VPackSlice const params, VPackBuilder& result);
  greenspun::EvalResult air_sendToAllNeighbours(greenspun::Machine& ctx,
                                                VPackSlice const params,
                                                VPackBuilder& result);

  greenspun::EvalResult air_globalAccumRef(greenspun::Machine& ctx,
                                           VPackSlice const params, VPackBuilder& result);
  greenspun::EvalResult air_sendToGlobalAccum(greenspun::Machine& ctx,
                                              VPackSlice const params, VPackBuilder& result);


  greenspun::EvalResult air_outboundEdges(greenspun::Machine& ctx,
                                          VPackSlice const params, VPackBuilder& result);
  greenspun::EvalResult air_neigbours(greenspun::Machine& ctx,
                                      VPackSlice const params, VPackBuilder& result);
  greenspun::EvalResult air_numberOutboundEdges(greenspun::Machine& ctx,
                                                VPackSlice const params,
                                                VPackBuilder& result);
  greenspun::EvalResult air_numberOfVertices(greenspun::Machine& ctx, VPackSlice const params,
                                             VPackBuilder& result);
  greenspun::EvalResult air_bindRef(greenspun::Machine& ctx,
                                    VPackSlice const params, VPackBuilder& result);

  // TODO: which of these IDs do we actually need?
  greenspun::EvalResult air_thisDoc(greenspun::Machine& ctx,
                                    VPackSlice const params, VPackBuilder& result);
  greenspun::EvalResult air_thisVertexId(greenspun::Machine& ctx,
                                    VPackSlice const params, VPackBuilder& result);
  greenspun::EvalResult air_thisUniqueId(greenspun::Machine& ctx,
                                    VPackSlice const params, VPackBuilder& result);
  greenspun::EvalResult air_thisPregelId(greenspun::Machine& ctx,
                                    VPackSlice const params, VPackBuilder& result);

  greenspun::EvalResult air_globalSuperstep(greenspun::Machine& ctx,
                                            VPackSlice const params, VPackBuilder& result);

 private:
  VertexAccumulators const& _algorithm;
  greenspun::Machine _airMachine;
};

}  // namespace accumulators
}  // namespace algos
}  // namespace pregel
}  // namespace arangodb
#endif
