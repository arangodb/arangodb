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

#include "Greenspun/Primitives.h"

using namespace arangodb::pregel;

namespace arangodb::pregel::algos::accumulators {

VertexComputation::VertexComputation(VertexAccumulators const& algorithm)
    : _algorithm(algorithm) {}

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

greenspun::EvalResult VertexComputation::runProgram(VertexComputationEvalContext& ctx,
                                                    VPackSlice program) {
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
  return result.wrapError([](greenspun::EvalError &err) {
    err.wrapMessage("at top-level");
  });
}

void VertexComputation::compute(MessageIterator<MessageData> const& incomingMessages) {
  auto evalContext = VertexComputationEvalContext(*this);

  auto phase_index = *getAggregatedValue<uint32_t>("phase");
  auto phase = _algorithm.options().phases.at(phase_index);

  LOG_DEVEL << "running phase " << phase.name
            << " superstep = " << phaseGlobalSuperstep()
            << " global superstep = " << globalSuperstep() << " at vertex "
            << vertexData()._vertexId;

  std::size_t phaseStep = phaseGlobalSuperstep();

  if (phaseStep == 0) {
    if (auto res = runProgram(evalContext, phase.initProgram.slice()); res.fail()) {
      getReportManager()
              .report(ReportLevel::ERROR)
              .with("pregel-id", pregelId())
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

    if (auto res = runProgram(evalContext, phase.updateProgram.slice()); res.fail()) {
      getReportManager()
              .report(ReportLevel::ERROR)
              .with("pregel-id", pregelId())
              .with("phase", phase.name)
              .with("global-superstep", globalSuperstep())
              .with("phase-step", phaseGlobalSuperstep())
          << "in phase `" << phase.name
          << "` update-program failed: " << res.error().toString();
    }
  }
}

}  // namespace arangodb::pregel::algos::accumulators
