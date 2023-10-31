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

#include <cstdint>
#include "Pregel/Algorithm.h"

namespace arangodb {
namespace pregel {
namespace algos {

struct SSSPType {
  using Vertex = int64_t;
  using Edge = int64_t;
  using Message = int64_t;
};

/// Single Source Shortest Path. Uses integer attribute 'value', the source
/// should have
/// the value == 0, all others -1 or an undefined value
class SSSPAlgorithm : public Algorithm<int64_t, int64_t, int64_t> {
  std::string _sourceDocumentId, _resultField = "result";

 public:
  explicit SSSPAlgorithm(VPackSlice userParams);

  [[nodiscard]] auto name() const -> std::string_view override {
    return "sssp";
  };

  std::shared_ptr<GraphFormat<int64_t, int64_t> const> inputFormat()
      const override;

  MessageFormat<int64_t>* messageFormat() const override {
    return new IntegerMessageFormat<int64_t>();
  }
  [[nodiscard]] auto messageFormatUnique() const
      -> std::unique_ptr<message_format> override {
    return std::make_unique<IntegerMessageFormat<int64_t>>();
  }

  MessageCombiner<int64_t>* messageCombiner() const override {
    return new MinCombiner<int64_t>();
  }
  [[nodiscard]] auto messageCombinerUnique() const
      -> std::unique_ptr<message_combiner> override {
    return std::make_unique<MinCombiner<int64_t>>();
  }

  VertexComputation<int64_t, int64_t, int64_t>* createComputation(
      std::shared_ptr<WorkerConfig const>) const override;
  VertexCompensation<int64_t, int64_t, int64_t>* createCompensation(
      std::shared_ptr<WorkerConfig const>) const override;

  size_t messageBatchSize(std::shared_ptr<WorkerConfig const> config,
                          MessageStats const& stats) const override;

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
}  // namespace algos
}  // namespace pregel
}  // namespace arangodb
