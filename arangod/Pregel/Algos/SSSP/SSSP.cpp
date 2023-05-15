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

#include "SSSP.h"
#include "Pregel/Algorithm.h"
#include "Pregel/IncomingCache.h"
#include "Pregel/MasterContext.h"
#include "Pregel/VertexComputation.h"

using namespace arangodb;
using namespace arangodb::pregel;
using namespace arangodb::pregel::algos;

struct SSSPComputation : public VertexComputation<int64_t, int64_t, int64_t> {
  SSSPComputation() {}
  void compute(MessageIterator<int64_t> const& messages) override {
    int64_t tmp = vertexData();
    for (const int64_t* msg : messages) {
      if (*msg < tmp) {
        tmp = *msg;
      };
    }
    int64_t* state = mutableVertexData();
    if (tmp < *state || (tmp == 0 && localSuperstep() == 0)) {
      *state = tmp;  // update state

      for (auto& edge : getEdges()) {
        int64_t val = edge.data() + tmp;
        sendMessage(edge, val);
      }
    }
    voteHalt();
  }
};

SSSPAlgorithm::SSSPAlgorithm(VPackSlice userParams) {
  if (!userParams.isObject() || !userParams.hasKey("source")) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER, "You need to specify the source document id");
  }
  _sourceDocumentId = userParams.get("source").copyString();

  VPackSlice slice = userParams.get("resultField");
  if (slice.isString()) {
    _resultField = slice.copyString();
  } else {
    VPackSlice slice = userParams.get("_resultField");
    if (slice.isString()) {
      _resultField = slice.copyString();
    }
  }
}
size_t SSSPAlgorithm::messageBatchSize(
    std::shared_ptr<WorkerConfig const> config,
    MessageStats const& stats) const {
  if (config->localSuperstep() <= 1) {
    return 5;
  } else {
    double msgsPerSec = stats.sendCount / stats.superstepRuntimeSecs;
    msgsPerSec /= config->parallelism();  // per thread
    return msgsPerSec > 100.0 ? (uint32_t)msgsPerSec : 100;
  }
}

VertexComputation<int64_t, int64_t, int64_t>* SSSPAlgorithm::createComputation(
    std::shared_ptr<WorkerConfig const> config) const {
  return new SSSPComputation();
}

struct SSSPGraphFormat : public InitGraphFormat<int64_t, int64_t> {
  std::string _sourceDocId, resultField;

 public:
  SSSPGraphFormat(std::string const& source, std::string const& result)
      : InitGraphFormat<int64_t, int64_t>(result, 0, 1), _sourceDocId(source) {}

  void copyVertexData(arangodb::velocypack::Options const&,
                      std::string const& documentId,
                      arangodb::velocypack::Slice document, int64_t& targetPtr,
                      uint64_t vertexId) const override {
    targetPtr = (documentId == _sourceDocId) ? 0 : INT64_MAX;
  }
};

std::shared_ptr<GraphFormat<int64_t, int64_t> const>
SSSPAlgorithm::inputFormat() const {
  return std::make_shared<SSSPGraphFormat>(_sourceDocumentId, _resultField);
}

struct SSSPCompensation : public VertexCompensation<int64_t, int64_t, int64_t> {
  SSSPCompensation() {}
  void compensate(bool inLostPartition) override {
    if (inLostPartition) {
      int64_t* data = mutableVertexData();
      *data = INT64_MAX;
    }
    voteActive();
  }
};

VertexCompensation<int64_t, int64_t, int64_t>*
SSSPAlgorithm::createCompensation(
    std::shared_ptr<WorkerConfig const> config) const {
  return new SSSPCompensation();
}

struct SSSPWorkerContext : public WorkerContext {
  SSSPWorkerContext(std::unique_ptr<AggregatorHandler> readAggregators,
                    std::unique_ptr<AggregatorHandler> writeAggregators)
      : WorkerContext(std::move(readAggregators),
                      std::move(writeAggregators)){};
};
[[nodiscard]] auto SSSPAlgorithm::workerContext(
    std::unique_ptr<AggregatorHandler> readAggregators,
    std::unique_ptr<AggregatorHandler> writeAggregators,
    velocypack::Slice userParams) const -> WorkerContext* {
  return new SSSPWorkerContext(std::move(readAggregators),
                               std::move(writeAggregators));
}
[[nodiscard]] auto SSSPAlgorithm::workerContextUnique(
    std::unique_ptr<AggregatorHandler> readAggregators,
    std::unique_ptr<AggregatorHandler> writeAggregators,
    velocypack::Slice userParams) const -> std::unique_ptr<WorkerContext> {
  return std::make_unique<SSSPWorkerContext>(std::move(readAggregators),
                                             std::move(writeAggregators));
}

struct SSSPMasterContext : public MasterContext {
  SSSPMasterContext(uint64_t vertexCount, uint64_t edgeCount,
                    std::unique_ptr<AggregatorHandler> aggregators)
      : MasterContext(vertexCount, edgeCount, std::move(aggregators)){};
};
[[nodiscard]] auto SSSPAlgorithm::masterContext(
    std::unique_ptr<AggregatorHandler> aggregators,
    arangodb::velocypack::Slice userParams) const -> MasterContext* {
  return new SSSPMasterContext(0, 0, std::move(aggregators));
}
[[nodiscard]] auto SSSPAlgorithm::masterContextUnique(
    uint64_t vertexCount, uint64_t edgeCount,
    std::unique_ptr<AggregatorHandler> aggregators,
    arangodb::velocypack::Slice userParams) const
    -> std::unique_ptr<MasterContext> {
  return std::make_unique<SSSPMasterContext>(vertexCount, edgeCount,
                                             std::move(aggregators));
}
