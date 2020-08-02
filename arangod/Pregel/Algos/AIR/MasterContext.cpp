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

#include <Logger/LogMacros.h>
#include <Logger/Logger.h>

#include "MasterContext.h"

namespace arangodb::pregel::algos::accumulators {

MasterContext::MasterContext(VertexAccumulators const* algorithm)
    : _algo(algorithm) {
  InitMachine(_airMachine);

  _airMachine.setFunction("goto-phase",
                          std::bind(&MasterContext::air_GotoPhase, this, std::placeholders::_1,
                                    std::placeholders::_2, std::placeholders::_3));
}

greenspun::EvalResult MasterContext::air_GotoPhase(greenspun::Machine& ctx,
                                                   VPackSlice const params,
                                                   VPackBuilder& result) {
  if (params.length() == 1) {
    VPackSlice v = params.at(0);
    if (v.isString()) {
      if (!gotoPhase(v.stringView())) {
        return greenspun::EvalError("Unknown phase `" +
                                    std::string{v.stringView()} + "`");
      };
    }
  }

  return greenspun::EvalError("expect single string argument");
}

greenspun::EvalResult MasterContext::air_Finish(greenspun::Machine& ctx,
                                                VPackSlice const params,
                                                VPackBuilder& result) {
  if (params.isEmptyArray()) {
    finish();
    return {};
  }

  return greenspun::EvalError("expect no arguments");
}

greenspun::EvalResult MasterContext::air_VertexCount(greenspun::Machine& ctx,
                                                     VPackSlice const params,
                                                     VPackBuilder& result) {
  if (!params.isEmptyArray()) {
    return greenspun::EvalError("expected no argument");
  }
  result.add(VPackValue(vertexCount()));
  return {};
}

greenspun::EvalResult MasterContext::air_AccumRef(greenspun::Machine& ctx,
                                                  VPackSlice const params,
                                                  VPackBuilder& result) {
  auto&& [accumId] = unpackTuple<std::string_view>(params);
  std::string globalName = "[global]-";
  globalName += accumId;
  auto accum = getAggregator<VertexAccumulatorAggregator>(globalName);
  if (accum != nullptr) {
    accum->getAccumulator().getValueIntoBuilder(result);
    return {};
  }
  return greenspun::EvalError("global accumulator `" + std::string{accumId} +
                              "' not found");
}

greenspun::EvalResult MasterContext::air_AccumSet(greenspun::Machine& ctx,
                                                  VPackSlice const params,
                                                  VPackBuilder& result) {
  auto&& [accumId, value] = unpackTuple<std::string_view, VPackSlice>(params);
  std::string globalName = "[global]-";
  globalName += accumId;
  auto accum = getAggregator<VertexAccumulatorAggregator>(globalName);
  if (accum != nullptr) {
    accum->getAccumulator().setBySlice(value);
    return {};
  }

  return greenspun::EvalError("accumulator `" + std::string{accumId} +
                              "` not found");
}

bool MasterContext::gotoPhase(std::string_view nextPhase) {
  auto const& phases = _algo->options().phases;
  auto iter = std::find_if(phases.begin(), phases.end(),
                           [&](auto const& ph) { return ph.name == nextPhase; });
  if (iter == std::end(phases)) {
    return false;
  }
  LOG_DEVEL << "goto phase " << nextPhase;
  aggregate<uint32_t>("phase", iter - phases.begin());
  aggregate<uint64_t>("phase-first-step", globalSuperstep() + 1);
  userSelectedNext = ContinuationResult::ACTIVATE_ALL;
  return true;
}

void MasterContext::finish() {
  LOG_DEVEL << "onHalt decided that we have finished";
  userSelectedNext = ContinuationResult::ABORT;
}

MasterContext::ContinuationResult MasterContext::postGlobalSuperstep(bool allVertexesVotedHalt) {
  if (!allVertexesVotedHalt) {
    return ContinuationResult::DONT_CARE;
  }

  auto phase_index = *getAggregatedValue<uint32_t>("phase");
  auto phase = _algo->options().phases.at(phase_index);

  if (!phase.onHalt.isEmpty()) {
    VPackBuilder onHaltResult;

    userSelectedNext = ContinuationResult::DONT_CARE;
    LOG_DEVEL << "evaluating user defined program: " << phase.onHalt.slice().toJson();
    auto res = greenspun::Evaluate(_airMachine, phase.onHalt.slice(), onHaltResult);
    if (res.fail()) {
      LOG_TOPIC("ac23e", ERR, Logger::PREGEL)
        << "onHalt program of phase `" << phase.name <<
        "` returned and error: " << res.error().toString();
      return ContinuationResult::ABORT;
    }

    if (userSelectedNext == ContinuationResult::DONT_CARE) {
      LOG_TOPIC("ac23e", ERR, Logger::PREGEL)
          << "onHalt program of phase `" + phase.name +
                 "` did not specify how to continue";
      return ContinuationResult::ABORT;
    }

    return userSelectedNext;
  } else {
    // simply select next phase or end
    phase_index += 1;
    if (phase_index == _algo->options().phases.size()) {
      LOG_DEVEL << "phase ended, no on halt program. Finish algorithm "
                   "because it was the last phase";
      return ContinuationResult::ABORT;
    }

    LOG_DEVEL << "no on halt program for this phase, going to next phase " << phase_index;
    aggregate<uint32_t>("phase", phase_index);
    aggregate<uint64_t>("phase-first-step", globalSuperstep() + 1);
    return ContinuationResult::ACTIVATE_ALL;
  }
  // the phase is done, evalute the onHalt program for this phase
  // and look at the return value.

  return ContinuationResult::ABORT;
}

}  // namespace arangodb::pregel::algos::accumulators
