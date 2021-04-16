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

#ifndef ARANGODB_PREGEL_ALGOS_ACCUMULATORS_WORKERCONTEXT_H
#define ARANGODB_PREGEL_ALGOS_ACCUMULATORS_WORKERCONTEXT_H 1

#include <Pregel/WorkerContext.h>

#include <Pregel/Algos/AIR/AIR.h>
#include <Greenspun/Primitives.h>

namespace arangodb {
namespace pregel {
namespace algos {
namespace accumulators {

struct WorkerContext : public ::arangodb::pregel::WorkerContext {
  explicit WorkerContext(ProgrammablePregelAlgorithm const* algorithm);

  void preGlobalSuperstep(uint64_t gss) override;
  void preGlobalSuperstepMasterMessage(VPackSlice msg) override;
  void postGlobalSuperstep(uint64_t gss) override;
  void postGlobalSuperstepMasterMessage(VPackBuilder& msg) override;

  [[nodiscard]] greenspun::EvalResult sendToGlobalAccumulator(std::string accumId, VPackSlice message) const;

private:

  struct MutexAccumPair {
    std::unique_ptr<AccumulatorBase> accum;
    mutable std::mutex mutex;

    explicit MutexAccumPair(std::unique_ptr<AccumulatorBase> accum) : accum(std::move(accum)) {}
  };

  [[nodiscard]] std::unordered_map<std::string, MutexAccumPair> const& globalAccumulatorsUpdates() const;
  [[nodiscard]] std::unordered_map<std::string, std::unique_ptr<AccumulatorBase>> const& globalAccumulators() const;

  ProgrammablePregelAlgorithm const* _algo;

  // This map contains the values of the global accumulators
  // from the last GSS
  std::unordered_map<std::string, std::unique_ptr<AccumulatorBase>> _globalAccumulators;


  // This only holds the *deltas* for the global accumulators, i.e.
  // these accumulators are reset before every GSS, and their contents
  // are sent back to the conductor at the end of every GSS

  // This unordered_map is never changed during a superstep. Only the accumulators
  // are accessed by multiple different threads. They are guarded using an individual mutex.
  // See MutexAccumPair::mutex.
  std::unordered_map<std::string, MutexAccumPair> _globalAccumulatorsUpdates;
};

}  // namespace accumulators
}  // namespace algos
}  // namespace pregel
}  // namespace arangodb
#endif
