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
#include <cstdint>
#include <functional>
#include <utility>

#include "Basics/Common.h"
#include "Pregel/GraphFormat.h"
#include "Pregel/MessageCombiner.h"
#include "Pregel/MessageFormat.h"
#include "Pregel/Statistics.h"
#include "Pregel/Worker/WorkerConfig.h"
#include "Pregel/WorkerContext.h"

namespace arangodb {
namespace pregel {

template<typename V, typename E, typename M>
class VertexComputation;

template<typename V, typename E, typename M>
class VertexCompensation;

class IAggregator;
class WorkerConfig;
class MasterContext;

struct IAlgorithm {
  virtual ~IAlgorithm() = default;

  [[nodiscard]] virtual IAggregator* aggregator(std::string const& name) const {
    return nullptr;
  }

  [[deprecated]] [[nodiscard]] virtual auto masterContext(
      std::unique_ptr<AggregatorHandler> aggregators,
      arangodb::velocypack::Slice userParams) const -> MasterContext* = 0;
  [[nodiscard]] virtual auto masterContextUnique(
      uint64_t vertexCount, uint64_t edgeCount,
      std::unique_ptr<AggregatorHandler> aggregators,
      arangodb::velocypack::Slice userParams) const
      -> std::unique_ptr<MasterContext> = 0;

  [[deprecated]] [[nodiscard]] virtual auto workerContext(
      std::unique_ptr<AggregatorHandler> readAggregators,
      std::unique_ptr<AggregatorHandler> writeAggregators,
      velocypack::Slice userParams) const -> WorkerContext* = 0;

  [[nodiscard]] virtual auto name() const -> std::string_view = 0;
};

// specify serialization, whatever
template<typename V, typename E, typename M>
struct Algorithm : IAlgorithm {
 public:
  // Data used by the algorithm at every vertex
  using vertex_type = V;
  // Data used by the algorithm for every edge
  using edge_type = E;
  // Data sent along edges in steps
  using message_type = M;

  using graph_format = GraphFormat<vertex_type, edge_type>;
  using message_format = MessageFormat<message_type>;
  using message_combiner = MessageCombiner<message_type>;
  using vertex_computation =
      VertexComputation<vertex_type, edge_type, message_type>;
  using vertex_compensation =
      VertexCompensation<vertex_type, edge_type, message_type>;

 public:
  [[nodiscard]] virtual auto workerContextUnique(
      std::unique_ptr<AggregatorHandler> readAggregators,
      std::unique_ptr<AggregatorHandler> writeAggregators,
      velocypack::Slice userParams) const -> std::unique_ptr<WorkerContext> = 0;
  virtual std::shared_ptr<graph_format const> inputFormat() const = 0;
  [[deprecated]] virtual message_format* messageFormat() const = 0;
  [[nodiscard]] virtual auto messageFormatUnique() const
      -> std::unique_ptr<message_format> = 0;
  [[deprecated]] virtual message_combiner* messageCombiner() const {
    return nullptr;
  }
  [[nodiscard]] virtual auto messageCombinerUnique() const
      -> std::unique_ptr<message_combiner> {
    return nullptr;
  }
  virtual vertex_computation* createComputation(
      std::shared_ptr<WorkerConfig const>) const = 0;
  virtual vertex_compensation* createCompensation(
      std::shared_ptr<WorkerConfig const>) const {
    return nullptr;
  }
  virtual std::set<std::string> initialActiveSet() { return {}; }

  [[deprecated]] [[nodiscard]] virtual uint32_t messageBatchSize(
      std::shared_ptr<WorkerConfig const> config,
      MessageStats const& stats) const {
    if (config->localSuperstep() == 0) {
      return 500;
    } else {
      double msgsPerSec =
          static_cast<double>(stats.sendCount) / stats.superstepRuntimeSecs;
      msgsPerSec /= static_cast<double>(config->parallelism());  // per thread
      msgsPerSec *= 0.06;
      return msgsPerSec > 250.0 ? (uint32_t)msgsPerSec : 250;
    }
  }
};

template<typename V, typename E, typename M>
class SimpleAlgorithm : public Algorithm<V, E, M> {
 protected:
  std::string _sourceField, _resultField;

  SimpleAlgorithm(VPackSlice userParams) {
    arangodb::velocypack::Slice field = userParams.get("sourceField");
    _sourceField = field.isString() ? field.copyString() : "value";
    field = userParams.get("resultField");
    _resultField = field.isString() ? field.copyString() : "result";
  }
};
}  // namespace pregel
}  // namespace arangodb
