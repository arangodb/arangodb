////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Pregel/Algorithm.h"
#include "Pregel/Algos/EffectiveCloseness/ECValue.h"
#include "Pregel/Algos/EffectiveCloseness/HLLCounter.h"

namespace arangodb {
namespace pregel {
namespace algos {

struct EffectiveClosenessType {
  using Vertex = ECValue;
  using Edge = int8_t;
  using Message = HLLCounter;
};

/// Effective Closeness
struct EffectiveCloseness
    : public SimpleAlgorithm<ECValue, int8_t, HLLCounter> {
  explicit EffectiveCloseness(VPackSlice params)
      : SimpleAlgorithm<ECValue, int8_t, HLLCounter>(params) {}

  [[nodiscard]] auto name() const -> std::string_view override {
    return "effectivecloseness";
  };

  std::shared_ptr<GraphFormat<ECValue, int8_t> const> inputFormat()
      const override;
  MessageFormat<HLLCounter>* messageFormat() const override;
  MessageCombiner<HLLCounter>* messageCombiner() const override;

  VertexComputation<ECValue, int8_t, HLLCounter>* createComputation(
      std::shared_ptr<WorkerConfig const>) const override;

  [[nodiscard]] auto masterContext(
      std::unique_ptr<AggregatorHandler> aggregators,
      arangodb::velocypack::Slice userParams) const -> MasterContext* override;
  [[nodiscard]] auto masterContextUnique(
      uint64_t vertexCount, uint64_t edgeCount,
      std::unique_ptr<AggregatorHandler> aggregators,
      arangodb::velocypack::Slice userParams) const
      -> std::unique_ptr<MasterContext> override;
};
}  // namespace algos
}  // namespace pregel
}  // namespace arangodb
