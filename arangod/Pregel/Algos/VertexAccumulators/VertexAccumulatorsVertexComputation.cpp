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
#include "Greenspun/Primitives.h"

using namespace arangodb::pregel;
using namespace arangodb::pregel::algos;

struct MyEvalContext : PrimEvalContext {
  explicit MyEvalContext(VertexAccumulators::VertexComputation& computation, VertexData& vertexData)
    : _computation(computation), _vertexData(vertexData) {};

  std::string const& getThisId() const override { return thisId; }

  void getAccumulatorValue(std::string_view accumId, VPackBuilder& builder) const override {
    _vertexData._accumulators.at(std::string{accumId})->getIntoBuilder(builder);
  }

  void setAccumulator(std::string_view accumId, VPackSlice value) override {
    // FIXME: oh. my. god. in 2020. we copy string views.
    // FIXME error handling (such as accumulator not found, etc)
    _vertexData._accumulators.at(std::string{accumId})->setBySlice(value);
  }

  // Need edge
  void updateAccumulator(std::string_view accumId, std::string_view vertexId,
                         VPackSlice value) override {
    //  sendMessage();
    MessageData msg;
    msg.reset(std::string{accumId}, value);

    RangeIterator<Edge<EdgeData>> edgeIter = _computation.getEdges();

    // Fix fix fix
    LOG_DEVEL << "sending a message now: " << accumId << " " << value.toJson();
    _computation.sendMessage(*edgeIter, msg);
  }

  void enumerateEdges(std::function<void(VPackSlice edge, VPackSlice vertex)> cb) const override {
    RangeIterator<Edge<EdgeData>> edgeIter = _computation.getEdges();
    for (; edgeIter.hasMore(); ++edgeIter) {
      VPackSlice edgeDoc = (*edgeIter)->data()._document.slice();
      // TODO: this we don't need, as it currently is in the document.
      VPackSlice toId = (*edgeIter)->data()._document.getKey("_to");

      cb(edgeDoc, toId);
    }
  }

  std::string thisId;

  VertexAccumulators::VertexComputation& _computation;
  VertexData& _vertexData;
};

VertexAccumulators::VertexComputation::VertexComputation(VertexAccumulators const& algorithm)
    : _algorithm(algorithm) {}

void VertexAccumulators::VertexComputation::compute(MessageIterator<MessageData> const& incomingMessages) {
  auto evalContext = MyEvalContext(*this, vertexData());

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

    voteHalt();
    /*
    if(somethingHasChanged) {
      voteActive();
    } else {
      voteHalt();
      }; */
  }
}
