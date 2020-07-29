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

#include <Logger/Logger.h>
#include <Logger/LogMacros.h>

#include "MasterContext.h"

namespace arangodb::pregel::algos::accumulators {

MasterContext::MasterContext(VertexAccumulators const* algorithm) : _algo(algorithm) {}

bool MasterContext::gotoPhase(std::string_view nextPhase) {
  auto const& phases = _algo->options().phases;
  auto iter = std::find_if(phases.begin(), phases.end(),
                           [&](auto const& ph) { return ph.name == nextPhase; });
  if (iter == std::end(phases)) {
    return false;
  }
  LOG_DEVEL << "goto phase " << nextPhase;
  aggregate<uint32_t>("phase", iter - phases.begin());
  aggregate<uint32_t>("phase-first-step", globalSuperstep() + 1);
  userSelectedNext = ContinuationResult::CONTINUE;
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
    VertexAccumulatorPhaseEvalContext ctx{*this};
    userSelectedNext = ContinuationResult::DONT_CARE;
    auto res = Evaluate(ctx, phase.onHalt.slice(), onHaltResult);
    if (res.fail()) {
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

}
