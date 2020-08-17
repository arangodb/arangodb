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

  _airMachine.setFunctionMember("goto-phase", &MasterContext::air_GotoPhase, this);
  _airMachine.setFunctionMember("finish", &MasterContext::air_Finish, this);
  _airMachine.setFunctionMember("vertex-count", &MasterContext::air_VertexCount, this);
  _airMachine.setFunctionMember("global-accum-ref", &MasterContext::air_AccumRef, this);
  _airMachine.setFunctionMember("global-accum-set!", &MasterContext::air_AccumSet, this);
  _airMachine.setFunctionMember("global-accum-clear!", &MasterContext::air_AccumClear, this);
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
      } else {
        return {};
      }
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

greenspun::EvalResult MasterContext::air_AccumClear(greenspun::Machine& ctx,
                                                    VPackSlice const params,
                                                    VPackBuilder& result) {
  auto&& [accumId] = unpackTuple<std::string_view>(params);
  std::string globalName = "[global]-";
  globalName += accumId;
  auto accum = getAggregator<VertexAccumulatorAggregator>(globalName);
  if (accum != nullptr) {
    accum->getAccumulator().clear();
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

  if (getReportManager().getNumErrors() > 0) {
    getReportManager().report(ReportLevel::INFO).with("phase", phase.name)
        << "stopping because of previous errors";
    return ContinuationResult::ERROR_ABORT;
  }

  if (!phase.onHalt.isEmpty()) {
    VPackBuilder onHaltResult;

    userSelectedNext = ContinuationResult::DONT_CARE;
    auto res = greenspun::Evaluate(_airMachine, phase.onHalt.slice(), onHaltResult);
    if (res.fail()) {
      getReportManager().report(ReportLevel::ERROR).with("phase", phase.name)
          << "onHalt program of phase `" << phase.name
          << "` returned and error: " << res.error().toString();
      return ContinuationResult::ABORT;
    }

    if (userSelectedNext == ContinuationResult::DONT_CARE) {
      getReportManager().report(ReportLevel::ERROR).with("phase", phase.name)
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
}

}  // namespace arangodb::pregel::algos::accumulators
