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
#include "Pregel/Algos/WCC/WCCValue.h"
#include "Pregel/SenderMessage.h"
#include "Pregel/SenderMessageFormat.h"

namespace arangodb::pregel::algos {

/// The idea behind the algorithm is very simple: propagate the smallest
/// vertex id along the edges to all vertices of a connected component. The
/// number of supersteps necessary is equal to the length of the maximum
/// diameter of all components + 1

struct WCCType {
  using Vertex = WCCValue;
  using Edge = uint64_t;
  using Message = SenderMessage<uint64_t>;
};

struct WCC
    : public SimpleAlgorithm<WCCValue, uint64_t, SenderMessage<uint64_t>> {
 public:
  explicit WCC(VPackSlice userParams) : SimpleAlgorithm(userParams) {}

  [[nodiscard]] auto name() const -> std::string_view override {
    return "wcc";
  };

  std::shared_ptr<GraphFormat<WCCValue, uint64_t> const> inputFormat()
      const override;

  MessageFormat<SenderMessage<uint64_t>>* messageFormat() const override {
    return new SenderMessageFormat<uint64_t>();
  }
  MessageCombiner<SenderMessage<uint64_t>>* messageCombiner() const override {
    return nullptr;
  }
  VertexComputation<WCCValue, uint64_t, SenderMessage<uint64_t>>*
      createComputation(std::shared_ptr<WorkerConfig const>) const override;

  [[nodiscard]] auto masterContext(
      std::unique_ptr<AggregatorHandler> aggregators,
      arangodb::velocypack::Slice userParams) const -> MasterContext* override;
  [[nodiscard]] auto masterContextUnique(
      uint64_t vertexCount, uint64_t edgeCount,
      std::unique_ptr<AggregatorHandler> aggregators,
      arangodb::velocypack::Slice userParams) const
      -> std::unique_ptr<MasterContext> override;
};
}  // namespace arangodb::pregel::algos
