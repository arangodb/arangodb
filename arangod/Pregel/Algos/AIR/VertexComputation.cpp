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

#include "VertexComputation.h"

#include <Pregel/Algos/AIR/AIR.h>
#include <Pregel/Algos/AIR/Greenspun/Interpreter.h>
#include <Pregel/Graph.h>

#include "AccumulatorAggregator.h"

#include "Greenspun/Primitives.h"

using namespace arangodb::pregel;

namespace arangodb::pregel::algos::accumulators {

VertexComputation::VertexComputation(VertexAccumulators const& algorithm)
    : _algorithm(algorithm) {
  InitMachine(_airMachine);

  registerLocalFunctions();
}

void VertexComputation::registerLocalFunctions() {
  _airMachine.setFunction("accum-ref",  // " name:id -> value:any ",
                          std::bind(&VertexComputation::air_accumRef, this,
                                    std::placeholders::_1, std::placeholders::_2,
                                    std::placeholders::_3));

  _airMachine.setFunction("accum-set!",  // " name:id -> value:any -> void ",
                          std::bind(&VertexComputation::air_accumSet, this,
                                    std::placeholders::_1, std::placeholders::_2,
                                    std::placeholders::_3));

  _airMachine.setFunction("accum-clear!",  // " name:id -> void ",
                          std::bind(&VertexComputation::air_accumClear, this,
                                    std::placeholders::_1, std::placeholders::_2,
                                    std::placeholders::_3));

  _airMachine.setFunction("bind-ref",  // " name:id -> value:any ",
                          std::bind(&VertexComputation::air_accumClear, this,
                                    std::placeholders::_1, std::placeholders::_2,
                                    std::placeholders::_3));

  _airMachine.setFunction("send-to-accum",  // " name:id -> to-vertex:pid -> value:any -> void ",
                          std::bind(&VertexComputation::air_sendToAccum, this,
                                    std::placeholders::_1, std::placeholders::_2,
                                    std::placeholders::_3));

  _airMachine.setFunction("send-to-all-neighbours",  // " name:id -> value:any -> void ",
                          std::bind(&VertexComputation::air_sendToAccum, this,
                                    std::placeholders::_1, std::placeholders::_2,
                                    std::placeholders::_3));

  _airMachine.setFunction("outbound-edges",  // " name:id -> value:any -> void ",
                          std::bind(&VertexComputation::air_outboundEdges, this,
                                    std::placeholders::_1, std::placeholders::_2,
                                    std::placeholders::_3));
}

greenspun::EvalResult VertexComputation::air_accumRef(greenspun::Machine& ctx,
                                                      VPackSlice const params,
                                                      VPackBuilder& result) {
  auto&& [accumId] = unpackTuple<std::string>(params);

  if (auto iter = vertexData()._accumulators.find(accumId);
      iter != std::end(vertexData()._accumulators)) {
    iter->second->getValueIntoBuilder(result);
    return {};
  } else {
    std::string globalName = "[global]-";
    globalName += accumId;
    auto accum =
        dynamic_cast<VertexAccumulatorAggregator const*>(getReadAggregator(globalName));

    if (accum != nullptr) {
      accum->getAccumulator().getValueIntoBuilder(result);
      return {};
    }
  }
  return greenspun::EvalError("accumulator `" + std::string{accumId} +
                              "` not found");
}

greenspun::EvalResult VertexComputation::air_accumSet(greenspun::Machine& ctx,
                                                      VPackSlice const params,
                                                      VPackBuilder& result) {
  auto&& [accumId, value] = unpackTuple<std::string, VPackSlice>(params);

  if (auto iter = vertexData()._accumulators.find(std::string{accumId});
      iter != std::end(vertexData()._accumulators)) {
    iter->second->setBySlice(value);
    return {};
  } else {
    std::string globalName = "[global]-";
    globalName += accumId;
    auto accum = dynamic_cast<VertexAccumulatorAggregator*>(getWriteAggregator(globalName));
    if (accum != nullptr) {
      LOG_DEVEL << "VertexComputation::air_accumSet " << accumId
                << " with value " << value.toJson();
      accum->getAccumulator().setBySlice(value);
      return {};
    }
  }

  return greenspun::EvalError("accumulator `" + std::string{accumId} +
                              "` not found");
}

greenspun::EvalResult VertexComputation::air_accumClear(greenspun::Machine& ctx,
                                                        VPackSlice const params,
                                                        VPackBuilder& result) {
  auto&& [accumId] = unpackTuple<std::string>(params);

  if (auto iter = vertexData()._accumulators.find(accumId);
      iter != std::end(vertexData()._accumulators)) {
    iter->second->clear();
    return {};
  } else {
    std::string globalName = "[global]-";
    globalName += accumId;
    auto accum = getAggregatedValue<VertexAccumulatorAggregator>(globalName);
    if (accum != nullptr) {
      accum->getAccumulator().clear();
      return {};
    }
  }
  return greenspun::EvalError("accumulator `" + std::string{accumId} +
                              "` not found");
}

greenspun::EvalResult VertexComputation::air_sendToAccum(greenspun::Machine& ctx,
                                                         VPackSlice const params,
                                                         VPackBuilder& result) {
  auto&& [destination, accumId, value] =
      unpackTuple<VPackSlice, std::string, VPackSlice>(params);

  auto const& accums = algorithm().options().vertexAccumulators;

  if (auto i = accums.find(std::string{accumId}); i != accums.end()) {
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

    PregelID id = pregelIdFromSlice(destination);

    MessageData msg;
    msg.reset(accumId, value, vertexData()._documentId);

    sendMessage(id, msg);
    return {};
  } else {
    std::string globalName = "[global]-";
    globalName += accumId;
    auto accum = dynamic_cast<VertexAccumulatorAggregator*>(
      getWriteAggregator(globalName));
    if (accum != nullptr) {
      LOG_DEVEL << "update global accum " << accumId << " with value " << value.toJson();
      accum->getAccumulator().updateByMessageSlice(value);
      return {};
    }
  }
  return greenspun::EvalError("vertex accumulator `" + std::string{accumId} +
                   "` not found");
}

greenspun::EvalResult VertexComputation::air_sendToAllNeighbours(
    greenspun::Machine& ctx, VPackSlice const params, VPackBuilder& result) {
  auto&& [accumId, value] = unpackTuple<std::string, VPackSlice>(params);
  MessageData msg;
  msg.reset(accumId, value, vertexData()._documentId);
  sendMessageToAllNeighbours(msg);
  return {};
}

greenspun::EvalResult VertexComputation::air_outboundEdges(greenspun::Machine& ctx,
                                                           VPackSlice const params,
                                                           VPackBuilder& result) {
  RangeIterator<Edge<EdgeData>> edgeIter = getEdges();
  VPackArrayBuilder edgesGuard(&result);

  // FIXME: Uglay
  // FIXME: For needs iterable support!
  for (; edgeIter.hasMore(); ++edgeIter) {
    VPackObjectBuilder objGuard(&result);
    result.add(VPackValue("to-pregel-id"));
    {
      VPackObjectBuilder pidGuard(&result);
      result.add(VPackValue("shard"));
      result.add(VPackValue((*edgeIter)->targetShard()));
      result.add(VPackValue("key"));
      result.add(VPackValue((*edgeIter)->toKey().toString()));
    }

    result.add(VPackValue("document"));
    VPackSlice edgeDoc = (*edgeIter)->data()._document.slice();
    result.add(edgeDoc);
  }

  return {};
}

greenspun::EvalResult VertexComputation::air_neigbours(greenspun::Machine& ctx,
                                                       VPackSlice const params,
                                                       VPackBuilder& result) {
  // FIXME: Implement
  LOG_DEVEL << "air_neighbours is not implemented yet";
  std::abort();
  return {};
}

greenspun::EvalResult VertexComputation::air_numberOutboundEdges(
    greenspun::Machine& ctx, VPackSlice const params, VPackBuilder& result) {
  result.add(VPackValue(getEdgeCount()));
  return {};
}

greenspun::EvalResult VertexComputation::air_numberOfVertices(greenspun::Machine& ctx,
                                                              VPackSlice const params,
                                                              VPackBuilder& result) {
  // FIXME: is this the total number of vertices?
  result.add(VPackValue(context()->vertexCount()));
  return {};
}

greenspun::EvalResult VertexComputation::air_bindRef(greenspun::Machine& ctx,
                                                     VPackSlice const params,
                                                     VPackBuilder& result) {
  auto const& bindings = algorithm().options().bindings;
  auto&& [bindId] = unpackTuple<std::string>(params);

  if (auto iter = bindings.find(bindId); iter != std::end(bindings)) {
    result.add(iter->second.slice());
    return {};
  }
  return greenspun::EvalError("Bind parameter `" + bindId + "` not found");
}

VertexAccumulators const& VertexComputation::algorithm() const {
  return _algorithm;
};

bool VertexComputation::processIncomingMessages(MessageIterator<MessageData> const& incomingMessages) {
  auto accumChanged = bool{false};

  for (const MessageData* msg : incomingMessages) {
    auto accumName = msg->_accumulatorName;
    auto&& accum = vertexData().accumulatorByName(accumName);

    accumChanged |= accum->updateByMessageSlice(msg->_value.slice()) ==
                    AccumulatorBase::UpdateResult::CHANGED;
  }

  return accumChanged;
}

void VertexComputation::runProgram(greenspun::Machine& ctx, VPackSlice program) {
  VPackBuilder resultBuilder;

  auto result = Evaluate(ctx, program, resultBuilder);
  if (!result) {
    LOG_DEVEL << "execution error: " << result.error().toString() << " voting to halt.";
    voteHalt();
  } else {
    auto rs = resultBuilder.slice();
    if (rs.isBoolean()) {
      if (rs.getBoolean()) {
        voteActive();
      } else {
        voteHalt();
      }
    } else {
      LOG_DEVEL << "program did not return a boolean value, but " << rs.toJson();
    }
  }
}

void VertexComputation::compute(MessageIterator<MessageData> const& incomingMessages) {
  auto phase_index = *getAggregatedValue<uint32_t>("phase");
  auto phase = _algorithm.options().phases.at(phase_index);

  LOG_DEVEL << "running phase " << phase.name
            << " superstep = " << phaseGlobalSuperstep()
            << " global superstep = " << globalSuperstep() << " at vertex "
            << vertexData()._vertexId;

  std::size_t phaseStep = phaseGlobalSuperstep();

  if (phaseStep == 0) {
    runProgram(_airMachine, phase.initProgram.slice());
  } else {
    auto accumChanged = processIncomingMessages(incomingMessages);
    if (!accumChanged && phaseStep != 1) {
      voteHalt();
      LOG_DEVEL << "No accumulators changed, voting halt";
      return;
    }

    runProgram(_airMachine, phase.updateProgram.slice());
  }
}

}  // namespace arangodb::pregel::algos::accumulators
