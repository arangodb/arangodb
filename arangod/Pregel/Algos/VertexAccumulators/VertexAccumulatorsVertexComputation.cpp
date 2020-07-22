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
    : _computation(computation), _vertexData(vertexData) {
  };

  std::string const& getThisId() const override { return _vertexData._documentId; }

  void getAccumulatorValue(std::string_view accumId, VPackBuilder& builder) const override {
    _vertexData._accumulators.at(std::string{accumId})->getIntoBuilder(builder);
  }

  void setAccumulator(std::string_view accumId, VPackSlice value) override {
    // FIXME: oh. my. god. in 2020. we copy string views.
    // FIXME error handling (such as accumulator not found, etc)
    _vertexData._accumulators.at(std::string{accumId})->setBySlice(value);
  }

  // Need edge
  void updateAccumulator(std::string_view accumId, std::string_view toId,
                         VPackSlice value) override {
    MessageData msg;
    msg.reset(std::string{accumId}, value);

    // FIXME: this is extremely stupid to do,
    //        once we have proper variables, we should
    //        carry Edge with us and use that directly
    auto edgeIter = _computation.getEdges();
    for (; edgeIter.hasMore(); ++edgeIter) {
      auto edge = *edgeIter;
      if (edge->data()._toId == toId) {
        _computation.sendMessage(edge, msg);
        return;
      }
    }
  }

  EvalResult enumerateEdges(std::function<EvalResult(VPackSlice edge)> cb) const override {
    RangeIterator<Edge<EdgeData>> edgeIter = _computation.getEdges();
    for (; edgeIter.hasMore(); ++edgeIter) {
      VPackSlice edgeDoc = (*edgeIter)->data()._document.slice();
      if (auto e = cb(edgeDoc); e.fail()) {
        return e.wrapError([&](EvalError& err) {
          err.wrapMessage("during edge enumeration");
        });
      }
    }

    return {};
  }

  EvalResult getBindingValue(std::string_view id, VPackBuilder& result) const override {
    if (auto bindParameter = _computation.algorithm().getBindParameter(id); bindParameter) {
      result.add(bindParameter.value());
      return {};
    }
    return EvalError("bind parameter `" + std::string{id} + "` not found");
  }

  VertexAccumulators::VertexComputation& _computation;
  VertexData& _vertexData;
};

VertexAccumulators::VertexComputation::VertexComputation(VertexAccumulators const& algorithm)
    : _algorithm(algorithm) {}

void VertexAccumulators::VertexComputation::compute(MessageIterator<MessageData> const& incomingMessages) {
  auto evalContext = MyEvalContext(*this, vertexData());

  if (globalSuperstep() == 0) {
    VPackBuilder initResultBuilder;

    auto result = Evaluate(evalContext, _algorithm.options().initProgram, initResultBuilder);
    if (!result) {
      LOG_DEVEL << "execution of initializer: " << result.error().toString();
    }

    Evaluate(evalContext, _algorithm.options().initProgram, initResultBuilder);
    // TODO: return value relevant? Maybe for activation?
  } else {
    for (const MessageData* msg : incomingMessages) {
      vertexData()._accumulators.at(msg->_accumulatorName)->updateBySlice(msg->_value.slice());
    }

    VPackBuilder stepResultBuilder;

    auto result = Evaluate(evalContext, _algorithm.options().updateProgram, stepResultBuilder);
    if(!result) {
      LOG_DEVEL << "execution of step: " << result.error().toString();
    }

    // voteHalt();
    /*
    if(somethingHasChanged) {
      voteActive();
    } else {
      voteHalt();
      }; */
  }
}
