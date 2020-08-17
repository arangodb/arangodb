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
#include "Greenspun/Extractor.h"

using namespace arangodb::pregel;

namespace arangodb::pregel::algos::accumulators {

VertexComputation::VertexComputation(VertexAccumulators const& algorithm)
    : _algorithm(algorithm) {
  InitMachine(_airMachine);

  registerLocalFunctions();
}

void VertexComputation::registerLocalFunctions() {
  _airMachine.setFunctionMember("accum-ref",  // " name:id -> value:any ",
                                &VertexComputation::air_accumRef, this);

  _airMachine.setFunctionMember("accum-set!",  // " name:id -> value:any -> void ",
                          &VertexComputation::air_accumSet, this);

  _airMachine.setFunctionMember("accum-clear!",  // " name:id -> void ",
                          &VertexComputation::air_accumClear, this);

  _airMachine.setFunctionMember("bind-ref",  // " name:id -> value:any ",
                          &VertexComputation::air_accumClear, this);

  _airMachine.setFunctionMember("send-to-accum",  // " name:id -> to-vertex:pid -> value:any -> void ",
                          &VertexComputation::air_sendToAccum, this);

  _airMachine.setFunctionMember("send-to-global-accum",  // " name:id -> value:any -> void ",
                                &VertexComputation::air_sendToGlobalAccum, this);

  _airMachine.setFunctionMember("send-to-all-neighbours",  // " name:id -> value:any -> void ",
                          &VertexComputation::air_sendToAllNeighbours, this);

  _airMachine.setFunctionMember("this-outbound-edges",  // " name:id -> value:any -> void ",
                          &VertexComputation::air_outboundEdges, this);

  _airMachine.setFunctionMember("this-outbound-edges-count", //,
                                &VertexComputation::air_numberOutboundEdges, this);

  _airMachine.setFunctionMember("this-doc",
                                &VertexComputation::air_thisDoc, this);

  _airMachine.setFunctionMember("this-vertex-id", // " () -> value:any ",
                                &VertexComputation::air_thisVertexId, this);

  _airMachine.setFunctionMember("this-unique-id", // ,
                                &VertexComputation::air_thisUniqueId, this);

  _airMachine.setFunctionMember("this-pregel-id", // ,
                                &VertexComputation::air_thisPregelId, this);

  _airMachine.setFunctionMember("vertex-count", // ,
                                &VertexComputation::air_numberOfVertices, this);

  _airMachine.setFunctionMember("global-superstep", //,
                                &VertexComputation::air_globalSuperstep, this);
}

greenspun::EvalResult VertexComputation::air_accumRef(greenspun::Machine& ctx,
                                                      VPackSlice const params,
                                                      VPackBuilder& result) {
  auto res = greenspun::extract<std::string>(params);
  if (res.fail()) {
    return std::move(res).error();
  }

  auto&& [accumId] = res.value();


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
  auto res = greenspun::extract<std::string, VPackSlice>(params);
  if (res.fail()) {
    return res.error();
  }

  auto&& [accumId, value] = res.value();

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
  auto res = greenspun::extract<std::string>(params);
  if (res.fail()) {
    return res.error();
  }
  auto&& [accumId] = res.value();

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
  auto res = greenspun::extract<VPackSlice, std::string, VPackSlice>(params);
  if (res.fail()) {
    return res.error();
  }
  auto&& [destination, accumId, value] = res.value();
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
  }
  return greenspun::EvalError("vertex accumulator `" + std::string{accumId} +
                              "` not found");
}

greenspun::EvalResult VertexComputation::air_sendToGlobalAccum(greenspun::Machine& ctx,
                                                               VPackSlice const params,
                                                               VPackBuilder& result) {
  auto res = greenspun::extract<std::string, VPackSlice>(params);
  if (res.fail()) {
    return res.error();
  }
  auto&& [accumId, value] = res.value();
  std::string globalName = "[global]-";
  globalName += accumId;
  auto accum = dynamic_cast<VertexAccumulatorAggregator*>(
    getWriteAggregator(globalName));
  if (accum != nullptr) {
    LOG_DEVEL << "update global accum " << accumId << " with value " << value.toJson();
    accum->getAccumulator().updateByMessageSlice(value);
    return {};
  }
  return greenspun::EvalError("vertex accumulator `" + std::string{accumId} +
                   "` not found");
}

greenspun::EvalResult VertexComputation::air_sendToAllNeighbours(
    greenspun::Machine& ctx, VPackSlice const params, VPackBuilder& result) {
  auto res = greenspun::extract<std::string, VPackSlice>(params);
  if (res.fail()) {
    return res.error();
  }
  auto&& [accumId, value] = res.value();
  MessageData msg;
  msg.reset(accumId, value, vertexData()._documentId);
  sendMessageToAllNeighbours(msg);
  return {};
}

greenspun::EvalResult VertexComputation::air_outboundEdges(greenspun::Machine& ctx,
                                                           VPackSlice const params,
                                                           VPackBuilder& result) {
  auto res = greenspun::extract<>(params);
  if (res.fail()) {
    return res.error();
  }
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
  return greenspun::EvalError("not implemented");
}

greenspun::EvalResult VertexComputation::air_numberOutboundEdges(
    greenspun::Machine& ctx, VPackSlice const params, VPackBuilder& result) {
  auto res = greenspun::extract<>(params);
  if (res.fail()) {
    return res.error();
  }
  result.add(VPackValue(getEdgeCount()));
  return {};
}

greenspun::EvalResult VertexComputation::air_numberOfVertices(greenspun::Machine& ctx,
                                                              VPackSlice const params,
                                                              VPackBuilder& result) {
  auto res = greenspun::extract<>(params);
  if (res.fail()) {
    return res.error();
  }
  // FIXME: is this the total number of vertices?
  result.add(VPackValue(context()->vertexCount()));
  return {};
}

greenspun::EvalResult VertexComputation::air_bindRef(greenspun::Machine& ctx,
                                                     VPackSlice const params,
                                                     VPackBuilder& result) {
  auto res = greenspun::extract<std::string>(params);
  if (res.fail()) {
    return res.error();
  }
  auto const& bindings = algorithm().options().bindings;
  auto&& [bindId] = res.value();

  if (auto iter = bindings.find(bindId); iter != std::end(bindings)) {
    result.add(iter->second.slice());
    return {};
  }
  return greenspun::EvalError("Bind parameter `" + bindId + "` not found");
}


greenspun::EvalResult VertexComputation::air_thisDoc(greenspun::Machine& ctx,
                                       VPackSlice const params, VPackBuilder& result) {
  result.add(this->vertexData()._document.slice());
  return {};
}

greenspun::EvalResult VertexComputation::air_thisVertexId(greenspun::Machine& ctx,
                                       VPackSlice const params, VPackBuilder& result) {
  result.add(VPackValue(this->vertexData()._documentId));
  return {};
}

greenspun::EvalResult VertexComputation::air_thisUniqueId(greenspun::Machine& ctx,
                                       VPackSlice const params, VPackBuilder& result) {
  // TODO: FIXME, WE DO NOT KNOW THIS TO BE UNIQUE, WE SHOULD PROBABLY BE USING THE DOCMENT
  //              ID AND COMPARE STRINGS OR SOMESUCH.
  // HACK HACK HACK HACK
  auto pid = pregelId();

  uint64_t combined = this->vertexData()._vertexId;
  combined <<= 16;
  combined |= pid.shard;

  LOG_DEVEL << "vertexId" << this->vertexData()._vertexId << " shard " << pid.shard;
  LOG_DEVEL << "combined " << combined;

  result.add(VPackValue(combined));
  return {};
}

greenspun::EvalResult VertexComputation::air_thisPregelId(greenspun::Machine& ctx,
                                       VPackSlice const params, VPackBuilder& result) {
  auto id = pregelId();
  {
    VPackObjectBuilder ob(&result);
    result.add("key", VPackValue(id.key));
    result.add("shard", VPackValue(id.shard));
  }

  return {};
}

greenspun::EvalResult VertexComputation::air_globalSuperstep(greenspun::Machine& ctx,
                                                              VPackSlice const params,
                                                              VPackBuilder& result) {
  result.add(VPackValue(globalSuperstep()));
  return {};
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

greenspun::EvalResult VertexComputation::runProgram(greenspun::Machine& ctx, VPackSlice program) {
  VPackBuilder resultBuilder;

  // A valid pregel program can at the moment return one of five values: none, true,
  // false, "vote-halt", or "vote-active"
  //
  // if it returns none, false, or "vote-halt", then we voteHalt(), if it returns
  // true or "vote-active" we voteActive()
  //
  // In all other cases we throw an error
  auto evaluateResult = [this](VPackSlice& rs) -> greenspun::EvalResult {
    if (rs.isNone()) {
      this->voteHalt();
      return {};
    } else if (rs.isBoolean()) {
      if (rs.getBoolean()) {
        this->voteActive();
        return {};
      } else {
        this->voteHalt();
        return {};
      }
    } else if (rs.isString()) {
      if (rs.stringRef().equals("vote-active")) {
        this->voteActive();
        return {};
      } else if (rs.stringRef().equals("vote-halt")) {
        this->voteHalt();
        return {};
      }
    }
    // Not a valid value, vote to halt and return error
    voteHalt();
    return greenspun::EvalError("pregel program returned " + rs.toJson() +
                                ", expect one of `none`, `true`, `false`, "
                                "`\"vote-halt\", or `\"vote-active\"`");
  };

  auto evalResult = Evaluate(ctx, program, resultBuilder);
  if (!evalResult) {
    // An error occurred during execution, we vote halt and return the error
    voteHalt();
    return evalResult.mapError([](greenspun::EvalError& err) { err.wrapMessage("at top-level"); });
  } else {
    auto rs = resultBuilder.slice();
    return evaluateResult(rs);
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
    if (auto res = runProgram(_airMachine, phase.initProgram.slice()); res.fail()) {
      getReportManager()
              .report(ReportLevel::ERROR)
              .with("pregel-id", pregelId())
              .with("vertex", vertexData()._documentId)
              .with("phase", phase.name)
              .with("global-superstep", globalSuperstep())
              .with("phase-step", phaseGlobalSuperstep())
          << "in phase `" << phase.name
          << "` init-program failed: " << res.error().toString();
    }
  } else {
    auto accumChanged = processIncomingMessages(incomingMessages);
    if (!accumChanged && phaseStep != 1) {
      voteHalt();
      LOG_DEVEL << "No accumulators changed, voting halt";
      return;
    }

    if (auto res = runProgram(_airMachine, phase.updateProgram.slice()); res.fail()) {
      getReportManager()
              .report(ReportLevel::ERROR)
              .with("pregel-id", pregelId())
              .with("vertex", vertexData()._documentId)
              .with("phase", phase.name)
              .with("global-superstep", globalSuperstep())
              .with("phase-step", phaseGlobalSuperstep())
          << "in phase `" << phase.name
          << "` update-program failed: " << res.error().toString();
    }
  }
}

}  // namespace arangodb::pregel::algos::accumulators
