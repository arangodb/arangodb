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

#include "VertexAccumulators.h"

#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Pregel/Algorithm.h"
#include "Pregel/GraphStore.h"
#include "Pregel/IncomingCache.h"
#include "Pregel/VertexComputation.h"
#include "Pregel/MasterContext.h"
#include "Pregel/Aggregator.h"

#include "Pregel/Worker.h"

#include "Logger/Logger.h"

#include "Pregel/Algos/VertexAccumulators/AccumulatorOptionsDeserializer.h"

#include "Pregel/Algos/VertexAccumulators/Greenspun/Interpreter.h"

#include <Pregel/Algos/VertexAccumulators/Greenspun/Primitives.h>
#include <set>

#define LOG_VERTEXACC(logId, level) \
  LOG_DEVEL << "[VertexAccumulators] "

// LOG_TOPIC(logId, level, Logger::QUERIES) << "[VertexAccumulators] "


using namespace arangodb;
using namespace arangodb::pregel;
using namespace arangodb::pregel::algos;

std::ostream& arangodb::pregel::algos::operator<<(std::ostream& os, VertexData const& vertexData) {
  os << vertexData.toString();
  return os;
}

VertexAccumulators::VertexAccumulators(application_features::ApplicationServer& server,
                                       VPackSlice userParams)
    : Algorithm(server, "VertexAccumulators") {
  LOG_VERTEXACC("", DEBUG) << "algorithm constructor entry";

  LOG_VERTEXACC("", DEBUG) << "initializing Greenspun interpreter";
  InitInterpreter();

  LOG_VERTEXACC("", DEBUG) << "parsing user parameters";
  parseUserParams(userParams);

  LOG_VERTEXACC("", DEBUG) << "algorithm constructor done";
}

bool VertexAccumulators::supportsAsyncMode() const { return false; }
bool VertexAccumulators::supportsCompensation() const { return false; }

auto VertexAccumulators::createComputation(WorkerConfig const* config) const
    -> vertex_computation* {
  return new VertexAccumulators::VertexComputation(*this);
}

auto VertexAccumulators::inputFormat() const -> graph_format* {
  // TODO: The resultField needs to be configurable from the user's side
  return new GraphFormat(_server, _options.resultField, _options.accumulatorsDeclaration);
}

void VertexAccumulators::parseUserParams(VPackSlice userParams) {
  LOG_VERTEXACC("", DEBUG) << "parsing user params: " << userParams;
  auto result = parseVertexAccumulatorOptions(userParams);
  if(result) {
    _options = std::move(result).get();

    LOG_VERTEXACC("", DEBUG) << "declared accumulators";
    for (auto&& acc : _options.accumulatorsDeclaration) {
      LOG_VERTEXACC("", DEBUG) << acc.first << " " << acc.second;
    }
  } else {
    // What do we do on error? std::terminate()
    LOG_VERTEXACC("", DEBUG) << result.error().as_string();
    std::abort();
  }
}

VertexAccumulatorOptions const& VertexAccumulators::options() const {
  return _options;
}

bool VertexAccumulators::getBindParameter(std::string_view name, VPackBuilder& into) const {
  std::string nameStr(name); // TODO remove this in c++20 (then this method will be noexcept)
  if (auto iter = options().bindings.find(nameStr); iter != std::end(options().bindings)) {
    into.add(iter->second.slice());
    return true;
  }

  return false;
}


struct VertexAccumulatorsMasterContext : MasterContext {

  struct VertexAccumulatorPhaseEvalContext : PrimEvalContext {
    VertexAccumulatorPhaseEvalContext(VertexAccumulatorsMasterContext &mc) : PrimEvalContext(), masterContext(mc) {}

    EvalResult gotoPhase(std::string_view nextPhase) const override {
      if (masterContext.gotoPhase(nextPhase)) {
        return {};
      }
      return EvalError("Unknown phase `" + std::string{nextPhase} + "`");
    }

    EvalResult finishAlgorithm() const override {
      masterContext.finish();
      return {};
    }

    EvalResult getVertexCount(VPackBuilder& result) override {
      result.add(VPackValue(masterContext.vertexCount()));
      return {};
    }

    VertexAccumulatorsMasterContext &masterContext;
  };

  VertexAccumulatorsMasterContext(VertexAccumulators const* algo)
      : _algo(algo) {}

  ContinuationResult userSelectedNext = ContinuationResult::DONT_CARE;

  bool gotoPhase(std::string_view nextPhase) {
    auto const& phases = _algo->options().phases;
    auto iter = std::find_if(phases.begin(), phases.end(),
                             [&](auto const& ph) { return ph.name == nextPhase; });
    if (iter == std::end(phases)) {
      return false;
    }
    LOG_DEVEL << "goto phase " << nextPhase;
    aggregate<uint32_t>("phase", iter - phases.begin());
    aggregate<uint64_t>("phase-first-step", globalSuperstep()+1);
    userSelectedNext = ContinuationResult::ACTIVATE_ALL;
    return true;
  }

  void finish() {
    LOG_DEVEL << "onHalt decided that we have finished";
    userSelectedNext = ContinuationResult::ABORT;
  }

  ContinuationResult postGlobalSuperstep(bool allVertexesVotedHalt) override {
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
      if(phase_index == _algo->options().phases.size()) {
        LOG_DEVEL << "phase ended, no on halt program. Finish algorithm because it was the last phase";
        return ContinuationResult::ABORT;
      }

      LOG_DEVEL << "no on halt program for this phase, going to next phase " << phase_index;
      aggregate<uint32_t>("phase", phase_index);
      aggregate<uint64_t>("phase-first-step", globalSuperstep()+1);
      return ContinuationResult::ACTIVATE_ALL;
    }
    // the phase is done, evalute the onHalt program for this phase
    // and look at the return value.

    return ContinuationResult::ABORT;
  }

  VertexAccumulators const* _algo;
};

MasterContext* VertexAccumulators::masterContext(VPackSlice userParams) const {
  return new VertexAccumulatorsMasterContext(this);
}

IAggregator* VertexAccumulators::aggregator(const std::string& name) const {
  if (name == "phase") {  // permanent value
    return new OverwriteAggregator<uint32_t>(0, true);
  } else if (name == "phase-first-step") {
    return new OverwriteAggregator<uint64_t>(0, true);
  }
  return nullptr;
}

template class arangodb::pregel::Worker<VertexData, EdgeData, MessageData>;
