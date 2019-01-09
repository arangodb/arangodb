////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_PREGEL_ALGOS_EFFECTIVE_CLSNESS_H
#define ARANGODB_PREGEL_ALGOS_EFFECTIVE_CLSNESS_H 1

#include "Pregel/Algorithm.h"
#include "Pregel/CommonFormats.h"

namespace arangodb {
namespace pregel {
namespace algos {

/// Effective Closeness
struct EffectiveCloseness : public SimpleAlgorithm<ECValue, int8_t, HLLCounter> {
  explicit EffectiveCloseness(VPackSlice params)
      : SimpleAlgorithm<ECValue, int8_t, HLLCounter>("EffectiveCloseness", params) {}

  GraphFormat<ECValue, int8_t>* inputFormat() const override;
  MessageFormat<HLLCounter>* messageFormat() const override;
  MessageCombiner<HLLCounter>* messageCombiner() const override;

  VertexComputation<ECValue, int8_t, HLLCounter>* createComputation(WorkerConfig const*) const override;
};
}  // namespace algos
}  // namespace pregel
}  // namespace arangodb
#endif
