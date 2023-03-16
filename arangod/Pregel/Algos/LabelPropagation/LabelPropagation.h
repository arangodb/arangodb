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
#include "Pregel/Algos/LabelPropagation/LPValue.h"

namespace arangodb::pregel::algos {

/// Finds communities in a network
/// Tries to assign every vertex to the community in which most of it's
/// neighbours are.
/// Each vertex sends the community ID to all neighbours. Stores the ID he
/// received
/// most frequently. Tries to avoid osscilation, usually won't converge so
/// specify a
/// maximum superstep number.

struct LabelPropagationType {
  using Vertex = LPValue;
  using Edge = int8_t;
  using Message = uint64_t;
};

struct LabelPropagation : public SimpleAlgorithm<LPValue, int8_t, uint64_t> {
 public:
  explicit LabelPropagation(VPackSlice userParams)
      : SimpleAlgorithm<LPValue, int8_t, uint64_t>(userParams) {}

  [[nodiscard]] auto name() const -> std::string_view override {
    return "labelpropagation";
  };

  std::shared_ptr<GraphFormat<LPValue, int8_t> const> inputFormat()
      const override;
  MessageFormat<uint64_t>* messageFormat() const override {
    return new NumberMessageFormat<uint64_t>();
  }
  [[nodiscard]] auto messageFormatUnique() const
      -> std::unique_ptr<message_format> override {
    return std::make_unique<NumberMessageFormat<uint64_t>>();
  }

  VertexComputation<LPValue, int8_t, uint64_t>* createComputation(
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
