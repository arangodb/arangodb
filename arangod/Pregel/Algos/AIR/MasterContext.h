////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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

#ifndef ARANGODB_PREGEL_ALGOS_ACCUMULATORS_MASTERCONTEXT_H
#define ARANGODB_PREGEL_ALGOS_ACCUMULATORS_MASTERCONTEXT_H 1

#include <Pregel/MasterContext.h>

#include <Greenspun/Primitives.h>
#include <Pregel/Algos/AIR/AIR.h>

namespace arangodb {
namespace pregel {
namespace algos {
namespace accumulators {

struct MasterContext : ::arangodb::pregel::MasterContext {
  explicit MasterContext(ProgrammablePregelAlgorithm const* algorithm);

  MasterContext(MasterContext&&) = delete;
  MasterContext(MasterContext const&) = delete;
  MasterContext& operator=(MasterContext&&) = delete;
  MasterContext& operator=(MasterContext const&) = delete;

  greenspun::EvalResult air_GotoPhase(greenspun::Machine& ctx, VPackSlice params, VPackBuilder& result);
  greenspun::EvalResult air_Finish(greenspun::Machine& ctx, VPackSlice params, VPackBuilder& result);
  greenspun::EvalResult air_VertexCount(greenspun::Machine& ctx, VPackSlice params, VPackBuilder& result);
  greenspun::EvalResult air_AccumRef(greenspun::Machine& ctx, VPackSlice params, VPackBuilder& result);
  greenspun::EvalResult air_AccumSet(greenspun::Machine& ctx, VPackSlice params, VPackBuilder& result);
  greenspun::EvalResult air_AccumClear(greenspun::Machine& ctx, VPackSlice params, VPackBuilder& result);
  greenspun::EvalResult air_GlobalSuperstep(greenspun::Machine& ctx, VPackSlice params, VPackBuilder& result);

  bool gotoPhase(std::string_view nextPhase);
  void finish();

  bool preGlobalSuperstepWithResult() override;
  ContinuationResult postGlobalSuperstep(bool allVertexesVotedHalt) override;
  void preGlobalSuperstepMessage(VPackBuilder& msg) override;
  bool postGlobalSuperstepMessage(VPackSlice workerMsgs) override;

  void serializeValues(VPackBuilder& msg) override;

  std::map<std::string, std::unique_ptr<AccumulatorBase>, std::less<>> const& globalAccumulators();
private:
  ProgrammablePregelAlgorithm const* _algo;
  greenspun::Machine _airMachine;

  std::map<std::string, std::unique_ptr<AccumulatorBase>, std::less<>> _globalAccumulators;

  ContinuationResult _userSelectedNext = ContinuationResult::DONT_CARE;
};

}  // namespace accumulators
}  // namespace algos
}  // namespace pregel
}  // namespace arangodb

#endif
