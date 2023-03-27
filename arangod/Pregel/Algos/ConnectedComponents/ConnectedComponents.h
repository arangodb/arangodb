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

namespace arangodb::pregel::algos {

struct ConnectedComponentsType {
  using Vertex = uint64_t;
  using Edge = uint8_t;
  using Message = uint64_t;
};

/// The idea behind the algorithm is very simple: propagate the smallest
/// vertex id along the edges to all vertices of a connected component. The
/// number of supersteps necessary is equal to the length of the maximum
/// diameter of all components + 1
/// doesn't necessarily leads to a correct result on unidirected graphs
struct ConnectedComponents
    : public SimpleAlgorithm<uint64_t, uint8_t, uint64_t> {
 public:
  explicit ConnectedComponents(VPackSlice userParams)
      : SimpleAlgorithm(userParams) {}

  [[nodiscard]] auto name() const -> std::string_view override {
    return "connectedcomponents";
  };

  std::shared_ptr<GraphFormat<uint64_t, uint8_t> const> inputFormat()
      const override;

  MessageFormat<uint64_t>* messageFormat() const override {
    return new IntegerMessageFormat<uint64_t>();
  }
  [[nodiscard]] auto messageFormatUnique() const
      -> std::unique_ptr<message_format> override {
    return std::make_unique<IntegerMessageFormat<uint64_t>>();
  }
  MessageCombiner<uint64_t>* messageCombiner() const override {
    return new MinCombiner<uint64_t>();
  }
  [[nodiscard]] auto messageCombinerUnique() const
      -> std::unique_ptr<message_combiner> override {
    return std::make_unique<MinCombiner<uint64_t>>();
  }
  VertexComputation<uint64_t, uint8_t, uint64_t>* createComputation(
      std::shared_ptr<WorkerConfig const>) const override;
  VertexCompensation<uint64_t, uint8_t, uint64_t>* createCompensation(
      std::shared_ptr<WorkerConfig const>) const override;

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
};
}  // namespace arangodb::pregel::algos
