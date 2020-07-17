////////////////////////////////////////////////////////////////////////////////
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
////////////////////////////////////////////////////////////////////////////////

#include <Pregel/Graph.h>
#include <Pregel/Algos/VertexAccumulators/Greenspun/Interpreter.h>
#include "VertexAccumulators.h"

using namespace arangodb::pregel;
using namespace arangodb::pregel::algos;

struct MyEvalContext : EvalContext {
  explicit MyEvalContext(VertexAccumulators::VertexComputation& computation) : _computation(computation) {};

  std::string const& getThisId() const override { return thisId; }

  VPackSlice getDocumentById(std::string_view id) const override {
    std::abort();
  }

  VPackSlice getAccumulatorValue(std::string_view id) const override {
    return VPackSlice::zeroSlice();
  }

  void setAccumulator(std::string_view accumId, VPackSlice value) override {
    std::abort();
  }

  void updateAccumulator(std::string_view accumId, std::string_view vertexId,
                         VPackSlice value) override {
    //  sendMessage();
    std::abort();
  }

  void enumerateEdges(std::function<void(VPackSlice edge, VPackSlice vertex)> cb) const override {
    RangeIterator<Edge<EdgeData>> edgeIter = _computation.getEdges();
    for (; edgeIter.hasMore(); ++edgeIter) {
      // FIXME: PLEASE!
      VPackBuilder yolo;
      yolo.add(VPackValue((*edgeIter)->toKey().toString()));
      cb(yolo.slice(), yolo.slice());
//      cb((*edgeIter)->data(), yolo.slice()));
    }
  }

  std::string thisId;
  VertexAccumulators::VertexComputation& _computation;
};

VertexAccumulators::VertexComputation::VertexComputation(VertexAccumulators const& algorithm)
    : _algorithm(algorithm) {}

void VertexAccumulators::VertexComputation::compute(MessageIterator<MessageData> const& incomingMessages) {
  auto evalContext = MyEvalContext(*this);
  //auto&& currentVertexData = vertexData();

  if (globalSuperstep() == 0) {
    VPackBuilder initResultBuilder;
    Evaluate(evalContext, _algorithm.options().initProgram, initResultBuilder);
    // TODO: return value relevant? Maybe for activation?
  } else {
    /* process incoming messages, update vertex accumulators */
    /* MessageData will contain updates for some vertex accumulators */
    for (const MessageData* msg : incomingMessages) {
      LOG_DEVEL << " a message " << msg;
    }

    VPackBuilder stepResultBuilder;
    Evaluate(evalContext, _algorithm.options().updateProgram, stepResultBuilder);

    /*
    if(somethingHasChanged) {
      voteActive();
    } else {
      voteHalt();
      }; */
  }
}
