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

#include <Pregel/Algos/VertexAccumulators/Greenspun/Interpreter.h>
#include <Pregel/Graph.h>
#include "Greenspun/Primitives.h"
#include "VertexAccumulators.h"

using namespace arangodb::pregel;
using namespace arangodb::pregel::algos;

struct VertexComputationEvalContext : PrimEvalContext {
  explicit VertexComputationEvalContext(VertexAccumulators::VertexComputation& computation, VertexData& vertexData)
      : _computation(computation), _vertexData(vertexData){};

  /* This is the _id of the vertex document */
  std::string const& getThisId() const override {
    return _vertexData._documentId;
  }

  /* What is this used for? */
  std::size_t getVertexUniqueId() const override {
    return _vertexData._vertexId;
  }

  /* what's done when print is called */
  void printCallback(const std::string &msg) const override {
    LOG_DEVEL << msg;
  }

  void getAccumulatorValue(std::string_view accumId, VPackBuilder& builder) const override {
    _vertexData._accumulators.at(std::string{accumId})->getValueIntoBuilder(builder);
  }

  void setAccumulator(std::string_view accumId, VPackSlice value) override {
    // FIXME: oh. my. god. in 2020. we copy string views.
    // FIXME error handling (such as accumulator not found, etc)
    _vertexData._accumulators.at(std::string{accumId})->setBySlice(value);
  }

  EvalResult getPregelId(VPackBuilder &result) const override {
    auto id = _computation.pregelId();
    {
      VPackObjectBuilder ob(&result);
      result.add("key", VPackValue(id.key));
      result.add("shard", VPackValue(id.shard));
    }
    return {};
  }

  void updateAccumulator(std::string_view accumId, std::string_view toId,
                         VPackSlice value) override {
    MessageData msg;
    msg.reset(std::string{accumId}, value, getThisId());

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

  EvalResult updateAccumulatorById(std::string_view accumId, VPackSlice toVertex, VPackSlice value) override {
    MessageData msg;
    msg.reset(std::string{accumId}, value, getThisId());

    const auto pregelIdFromSlice = [](VPackSlice slice) -> PregelID {
      if (slice.isObject()) {
        VPackSlice key = slice.get("key");
        VPackSlice shard = slice.get("shard");
        if (key.isString() && shard.isNumber<PregelShard>()) {
          return PregelID(shard.getNumber<PregelShard>(), key.copyString());
        }
      }

      return {};
    };

    PregelID id = pregelIdFromSlice(toVertex);
    _computation.sendMessage(id, msg);
    return {};
  }

  EvalResult enumerateEdges(std::function<EvalResult(VPackSlice edge)> cb) const override {
    RangeIterator<Edge<EdgeData>> edgeIter = _computation.getEdges();
    for (; edgeIter.hasMore(); ++edgeIter) {
      VPackSlice edgeDoc = (*edgeIter)->data()._document.slice();
      if (auto e = cb(edgeDoc); e.fail()) {
        return e.wrapError(
            [&](EvalError& err) { err.wrapMessage("during edge enumeration"); });
      }
    }

    return {};
  }

  EvalResult getBindingValue(std::string_view id, VPackBuilder& result) const override {
    if (_computation.algorithm().getBindParameter(id, result)) {
      return {};
    }
    return EvalError("bind parameter `" + std::string{id} + "` not found");
  }

  EvalResult getGlobalSuperstep(VPackBuilder &result) const override {
    result.add(VPackValue(_computation.phaseGlobalSuperstep()));
    return {};
  }

  VertexAccumulators::VertexComputation& _computation;
  VertexData& _vertexData;
};

VertexAccumulators::VertexComputation::VertexComputation(VertexAccumulators const& algorithm)
    : _algorithm(algorithm) {}

void VertexAccumulators::VertexComputation::compute(MessageIterator<MessageData> const& incomingMessages) {
  auto evalContext = VertexComputationEvalContext(*this, vertexData());

  auto phase_index = *getAggregatedValue<uint32_t>("phase");
  auto phase = _algorithm.options().phases.at(phase_index);

  LOG_DEVEL << "running phase " << phase.name << " superstep = " << phaseGlobalSuperstep() << " global superstep = " << globalSuperstep();

  std::size_t phaseStep = phaseGlobalSuperstep();

  if (phaseStep == 0) {
    VPackBuilder initResultBuilder;

    auto result = Evaluate(evalContext, phase.initProgram.slice(),
                           initResultBuilder);
    if (!result) {
      LOG_DEVEL << "execution of initializer: " << result.error().toString() << " voting to halt.";
      voteHalt();
    } else {
      auto rs = initResultBuilder.slice();
      if (rs.isBoolean()) {
        if (rs.getBoolean()) {
          voteActive();
        } else {
          voteHalt();
        }
      } else {
        LOG_DEVEL << "initProgram of phase `" << phase.name << "` did not return a boolean value, but " << rs.toJson();
      }
    }
  } else {
    bool accumChanged = false;
    for (const MessageData* msg : incomingMessages) {
      accumChanged |= vertexData()
          ._accumulators.at(msg->_accumulatorName)
          ->updateByMessageSlice(msg->_value.slice()) == AccumulatorBase::UpdateResult::CHANGED;
    }

    if (!accumChanged && phaseStep != 1) {
      voteHalt();
      LOG_DEVEL << "No accumulators changed, voting halt";
      return;
    }

    VPackBuilder stepResultBuilder;

    auto result = Evaluate(evalContext, phase.updateProgram.slice(),
                           stepResultBuilder);
    if (!result) {
      LOG_DEVEL << "execution of step: " << result.error().toString() << " voting to halt";
      voteHalt();
    } else {
      auto rs = stepResultBuilder.slice();
      if (rs.isBoolean()) {
        if (rs.getBoolean()) {
          voteActive();
        } else {
          voteHalt();
        }
      } else {
        LOG_DEVEL << "updateProgram of phase `" << phase.name << "` did not return a boolean value, but " << rs.toJson();
      }
    }
  }
}
