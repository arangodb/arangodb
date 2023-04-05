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

#include <velocypack/Slice.h>
#include "Pregel/Algorithm.h"

namespace arangodb {
namespace pregel {
namespace algos {

struct RecoveringPageRankType {
  using Vertex = float;
  using Edge = float;
  using Message = float;
};

/// PageRank
struct RecoveringPageRank : public SimpleAlgorithm<float, float, float> {
  explicit RecoveringPageRank(arangodb::velocypack::Slice params)
      : SimpleAlgorithm(params) {}

  [[nodiscard]] auto name() const -> std::string_view override {
    return "pagerank";
  };

  [[nodiscard]] auto workerContext(
      std::unique_ptr<AggregatorHandler> readAggregators,
      std::unique_ptr<AggregatorHandler> writeAggregators,
      velocypack::Slice userParams) const -> WorkerContext* override;
  [[nodiscard]] auto workerContextUnique(
      std::unique_ptr<AggregatorHandler> readAggregators,
      std::unique_ptr<AggregatorHandler> writeAggregators,
      velocypack::Slice userParams) const
      -> std::unique_ptr<WorkerContext> override;

  [[nodiscard]] auto masterContext(
      std::unique_ptr<AggregatorHandler> aggregators,
      arangodb::velocypack::Slice userParams) const -> MasterContext* override;
  [[nodiscard]] auto masterContextUnique(
      uint64_t vertexCount, uint64_t edgeCount,
      std::unique_ptr<AggregatorHandler> aggregators,
      arangodb::velocypack::Slice userParams) const
      -> std::unique_ptr<MasterContext> override;

  std::shared_ptr<GraphFormat<float, float> const> inputFormat()
      const override {
    return std::make_shared<VertexGraphFormat<float, float>>(_resultField,
                                                             0.0f);
  }

  MessageFormat<float>* messageFormat() const override {
    return new NumberMessageFormat<float>();
  }
  [[nodiscard]] auto messageFormatUnique() const
      -> std::unique_ptr<message_format> override {
    return std::make_unique<NumberMessageFormat<float>>();
  }

  MessageCombiner<float>* messageCombiner() const override {
    return new SumCombiner<float>();
  }
  [[nodiscard]] auto messageCombinerUnique() const
      -> std::unique_ptr<message_combiner> override {
    return std::make_unique<SumCombiner<float>>();
  }

  VertexComputation<float, float, float>* createComputation(
      std::shared_ptr<WorkerConfig const>) const override;
  VertexCompensation<float, float, float>* createCompensation(
      std::shared_ptr<WorkerConfig const>) const override;
  IAggregator* aggregator(std::string const& name) const override;
};
}  // namespace algos
}  // namespace pregel
}  // namespace arangodb
