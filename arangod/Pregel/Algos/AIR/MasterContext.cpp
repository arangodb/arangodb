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
  CustomAccumulatorDefinitions const& customDefinitions = _algo->options().customAccumulators;
  AccumulatorsDeclaration const& globalAccumulatorsDeclarations =
      _algo->options().globalAccumulators;

  for (auto&& decl : globalAccumulatorsDeclarations) {
    auto [acc, ignore] = _globalAccumulators.emplace(decl.first, instantiateAccumulator(decl.second, customDefinitions));
    // TODO: Clear could fail :/
    acc->second->clearWithResult();
  }

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

  if (auto iter = _globalAccumulators.find(accumId); iter != std::end(_globalAccumulators)) {
    auto inner = iter->second->getIntoBuilderWithResult(result);
    if (!inner) {
      return inner.error();
    } else {
      return {};
    }
  }
  return greenspun::EvalError("global accumulator `" + std::string{accumId} +
                              "` not found");
}

greenspun::EvalResult MasterContext::air_AccumSet(greenspun::Machine& ctx,
                                                  VPackSlice const params,
                                                  VPackBuilder& result) {
  auto&& [accumId, value] = unpackTuple<std::string_view, VPackSlice>(params);

  if (auto iter = _globalAccumulators.find(accumId); iter != std::end(_globalAccumulators)) {
    auto inner = iter->second->updateBySlice(value);
    if (!inner) {
      return inner.error();
    } else {
      return {};
    }
  }
  return greenspun::EvalError("global accumulator `" + std::string{accumId} +
                              "` not found");
}

greenspun::EvalResult MasterContext::air_AccumClear(greenspun::Machine& ctx,
                                                    VPackSlice const params,
                                                    VPackBuilder& result) {
  auto&& [accumId] = unpackTuple<std::string_view>(params);

  if (auto iter = _globalAccumulators.find(accumId); iter != std::end(_globalAccumulators)) {
    auto inner = iter->second->clearWithResult();
    if (!inner) {
      return inner.error();
    } else {
      return {};
    }
  }
  return greenspun::EvalError("global accumulator `" + std::string{accumId} +
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

void MasterContext::preGlobalSuperstepMessage(VPackBuilder& msg) {
  // Send the current values of all global accumulators to the workers
  // where they will be received and passed to WorkerContext in preGlobalSuperstepMessage
  {
    VPackObjectBuilder msgGuard(&msg);
    {
      VPackObjectBuilder valuesGuard(&msg, "globalAccumulatorValues");

      for (auto&& acc : globalAccumulators()) {
        msg.add(VPackValue(acc.first));

        if(auto result = acc.second->getIntoBuilderWithResult(msg); result.fail()) {
          LOG_DEVEL << "AIR MasterContext, error serializing global accumulator "
                    << acc.first << " " << result.error().toString();
        }
      }
    }
  }
}

bool MasterContext::postGlobalSuperstepMessage(VPackSlice workerMsgs) {
  if (!workerMsgs.isArray()) {
    LOG_DEVEL << "AIR MasterContext received invalid message from conductor: " << workerMsgs.toJson() << " expecting array of objects";
    return false;
  }

  for(auto&& msg : VPackArrayIterator(workerMsgs)) {
    if (!msg.isObject()) {
      LOG_DEVEL << "AIR MasterContext received invalid message from worker: " << msg.toJson() << " expecting object; stopping.";
      return false;
    }

    auto accumulatorUpdate = msg.get("globalAccumulatorUpdates");
    if (!accumulatorUpdate.isObject()) {
      LOG_DEVEL << "AIR MasterContext did not receive globalAccumulatorUpdates from worker.";
      continue;
    }
    for (auto&& upd : VPackObjectIterator(accumulatorUpdate)) {
      if (!upd.key.isString()) {
        LOG_DEVEL << "AIR MasterContext received invalid key from worker: " << upd .key.toJson() << " expecting string.";
        return false;
      }

      auto accumName = upd.key.copyString();
      if (auto iter = globalAccumulators().find(accumName); iter != std::end(globalAccumulators())) {
        auto res = iter->second->updateBySlice(upd.value);
        if (!res) {
          LOG_DEVEL << "AIR MasterContext could not update global accumulator " << accumName << ", " << res.error().toString();
          return false;
        }
      } else {
        LOG_DEVEL << "AIR MasterContext received update for unknown global accumulator " << accumName;
      }
    }
  }

  return true;
}


void MasterContext::serializeValues(VPackBuilder& msg) {
  {
    VPackObjectBuilder msgGuard(&msg);
    {
      VPackObjectBuilder valuesGuard(&msg, "globalAccumulatorValues");

      for (auto&& acc : globalAccumulators()) {
        msg.add(VPackValue(acc.first));

        if(auto result = acc.second->getIntoBuilderWithResult(msg); result.fail()) {
          LOG_DEVEL << "AIR MasterContext, error serializing global accumulator "
                    << acc.first << " " << result.error().toString();
        }
      }
    }
  }
}

std::map<std::string, std::unique_ptr<AccumulatorBase>, std::less<>> const& MasterContext::globalAccumulators() {
  return _globalAccumulators;
}

}  // namespace arangodb::pregel::algos::accumulators
