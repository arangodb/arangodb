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
/// @author Roman Rabinovich
////////////////////////////////////////////////////////////////////////////////

#include "ReadWrite.h"

#include <utility>
#include "Pregel/Aggregator.h"
#include "Pregel/GraphFormat.h"
#include "Pregel/Iterators.h"
#include "Pregel/MasterContext.h"
#include "Pregel/Utils.h"
#include "Pregel/VertexComputation.h"

using namespace arangodb;
using namespace arangodb::pregel;
using namespace arangodb::pregel::algos;

static std::string const simulatedAggregatorName = "simulatedAggregator";

struct ReadWriteWorkerContext : public WorkerContext {
  ReadWriteWorkerContext(std::unique_ptr<AggregatorHandler> readAggregators,
                         std::unique_ptr<AggregatorHandler> writeAggregators)
      : WorkerContext(std::move(readAggregators),
                      std::move(writeAggregators)){};
};

ReadWrite::ReadWrite(VPackSlice const& userParams)
    : SimpleAlgorithm(userParams) {}

struct ReadWriteGraphFormat final : public GraphFormat<V, E> {
  ReadWriteGraphFormat(std::string sourceFieldName, std::string resultFieldName)
      : GraphFormat(),
        sourceFieldName{std::move(sourceFieldName)},
        resultFieldName{std::move(resultFieldName)} {}
  std::string const sourceFieldName;
  std::string const resultFieldName;

  void copyVertexData(arangodb::velocypack::Options const&,
                      std::string const& documentId, VPackSlice document,
                      V& targetPtr, uint64_t vertexId) const override {
    auto value = document.get(sourceFieldName);
    if (value.isNone()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                     "Vertex with ID " + documentId +
                                         " has no property " + sourceFieldName +
                                         ".");
    } else {
      if (!value.isNumber()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                       "Vertex with ID " + documentId +
                                           " has property " + sourceFieldName +
                                           ", whose type is not a number.");
      }
      targetPtr = static_cast<V>(value.getNumber<V>());
    }
  }

  bool buildVertexDocument(arangodb::velocypack::Builder& b,
                           V const* ptr) const override {
    b.add(resultFieldName, arangodb::velocypack::Value(*ptr));
    return true;
  }
};

std::shared_ptr<GraphFormat<V, E> const> ReadWrite::inputFormat() const {
  return std::make_shared<ReadWriteGraphFormat>(_sourceField, _resultField);
}

struct ReadWriteComputation : public VertexComputation<V, E, V> {
  ReadWriteComputation() = default;
  void compute(MessageIterator<float> const& messages) override {
    V sum{};
    sum++;
    for (const float* msg : messages) {
      sum += *msg;
    }
    aggregate<float>(simulatedAggregatorName, sum);
    sendMessageToAllNeighbours(sum);
  }
};

VertexComputation<V, E, V>* ReadWrite::createComputation(
    std::shared_ptr<WorkerConfig const> config) const {
  return new ReadWriteComputation();
}

[[nodiscard]] auto ReadWrite::workerContext(
    std::unique_ptr<AggregatorHandler> readAggregators,
    std::unique_ptr<AggregatorHandler> writeAggregators,
    velocypack::Slice userParams) const -> WorkerContext* {
  return new ReadWriteWorkerContext(std::move(readAggregators),
                                    std::move(writeAggregators));
}
[[nodiscard]] auto ReadWrite::workerContextUnique(
    std::unique_ptr<AggregatorHandler> readAggregators,
    std::unique_ptr<AggregatorHandler> writeAggregators,
    velocypack::Slice userParams) const -> std::unique_ptr<WorkerContext> {
  return std::make_unique<ReadWriteWorkerContext>(std::move(readAggregators),
                                                  std::move(writeAggregators));
}

struct ReadWriteMasterContext : public MasterContext {
  size_t maxGss;
  explicit ReadWriteMasterContext(
      uint64_t vertexCount, uint64_t edgeCount,
      std::unique_ptr<AggregatorHandler> aggregators, VPackSlice userParams)
      : MasterContext(vertexCount, edgeCount, std::move(aggregators)) {
    auto value = userParams.get(Utils::maxGSS);
    if (not value.isNone()) {
      maxGss = value.getInt();
    }
  }

  bool postGlobalSuperstep() override { return globalSuperstep() <= maxGss; };
};

[[nodiscard]] auto ReadWrite::masterContext(
    std::unique_ptr<AggregatorHandler> aggregators,
    arangodb::velocypack::Slice userParams) const -> MasterContext* {
  return new ReadWriteMasterContext(0, 0, std::move(aggregators), userParams);
}
[[nodiscard]] auto ReadWrite::masterContextUnique(
    uint64_t vertexCount, uint64_t edgeCount,
    std::unique_ptr<AggregatorHandler> aggregators,
    arangodb::velocypack::Slice userParams) const
    -> std::unique_ptr<MasterContext> {
  return std::make_unique<ReadWriteMasterContext>(
      vertexCount, edgeCount, std::move(aggregators), userParams);
}

IAggregator* ReadWrite::aggregator(std::string const& name) const {
  if (name == simulatedAggregatorName) {
    return new MaxAggregator<V>(-1, false);
  }
  return nullptr;
}
