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

#include "Pregel/Algos/ShortestPath/ShortestPath.h"
#include "Aql/ExecutionBlockImpl.tpp"
#include "Pregel/Aggregator.h"
#include "Pregel/Algorithm.h"
#include "Pregel/MasterContext.h"
#include "Pregel/IncomingCache.h"
#include "Pregel/VertexComputation.h"
#include "Pregel/Worker/WorkerConfig.h"

using namespace arangodb;
using namespace arangodb::pregel;
using namespace arangodb::pregel::algos;

static std::string const spUpperPathBound = "bound";

struct SPComputation : public VertexComputation<int64_t, int64_t, int64_t> {
  VertexID _target;

  explicit SPComputation(VertexID const& target) : _target(target) {}
  void compute(MessageIterator<int64_t> const& messages) override {
    int64_t current = vertexData();
    for (const int64_t* msg : messages) {
      if (*msg < current) {
        current = *msg;
      };
    }

    // use global state to limit the computation of paths
    bool isSource = current == 0 && localSuperstep() == 0;
    auto const& max = getAggregatedValueRef<int64_t>(spUpperPathBound);

    int64_t* state = mutableVertexData();
    if (isSource || (current < *state && current < max)) {
      *state = current;  // update state

      if (this->pregelId() == _target) {
        // TODO extend pregel to update certain aggregators during a GSS
        aggregate(spUpperPathBound, current);
        LOG_TOPIC("0267f", DEBUG, Logger::PREGEL) << "Found target " << current;
        return;
      }

      for (auto& edge : getEdges()) {
        int64_t val = edge.data() + current;
        if (val < max) {
          sendMessage(edge, val);
        }
      }
    }

    voteHalt();
  }
};

struct arangodb::pregel::algos::SPGraphFormat
    : public InitGraphFormat<int64_t, int64_t> {
  std::string _sourceDocId, _targetDocId;

 public:
  SPGraphFormat(std::string const& source, std::string const& target)
      : InitGraphFormat<int64_t, int64_t>("length", 0, 1),
        _sourceDocId(source),
        _targetDocId(target) {}

  void copyVertexData(arangodb::velocypack::Options const&,
                      std::string const& documentId,
                      arangodb::velocypack::Slice document, int64_t& targetPtr,
                      uint64_t vertexIdRange) const override {
    targetPtr = (documentId == _sourceDocId) ? 0 : INT64_MAX;
  }
};

ShortestPathAlgorithm::ShortestPathAlgorithm(VPackSlice userParams) {
  VPackSlice val1 = userParams.get("source");
  VPackSlice val2 = userParams.get("target");
  if (val1.isNone() || val2.isNone()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "You must specify source and target");
  }
  _source = val1.copyString();
  _target = val2.copyString();
}

std::set<std::string> ShortestPathAlgorithm::initialActiveSet() {
  return std::set<std::string>{_source};
}

std::shared_ptr<GraphFormat<int64_t, int64_t> const>
ShortestPathAlgorithm::inputFormat() const {
  return std::make_shared<SPGraphFormat>(_source, _target);
}

VertexComputation<int64_t, int64_t, int64_t>*
ShortestPathAlgorithm::createComputation(
    std::shared_ptr<WorkerConfig const> _config) const {
  VertexID target = _config->documentIdToPregel(_target);
  return new SPComputation(target);
}

IAggregator* ShortestPathAlgorithm::aggregator(std::string const& name) const {
  if (name == spUpperPathBound) {  // persistent min operator
    return new MinAggregator<int64_t>(INT64_MAX, true);
  }
  return nullptr;
}

struct ShortestPathWorkerContext : public WorkerContext {
  ShortestPathWorkerContext(std::unique_ptr<AggregatorHandler> readAggregators,
                            std::unique_ptr<AggregatorHandler> writeAggregators)
      : WorkerContext(std::move(readAggregators),
                      std::move(writeAggregators)){};
};
[[nodiscard]] auto ShortestPathAlgorithm::workerContext(
    std::unique_ptr<AggregatorHandler> readAggregators,
    std::unique_ptr<AggregatorHandler> writeAggregators,
    velocypack::Slice userParams) const -> WorkerContext* {
  return new ShortestPathWorkerContext(std::move(readAggregators),
                                       std::move(writeAggregators));
}
[[nodiscard]] auto ShortestPathAlgorithm::workerContextUnique(
    std::unique_ptr<AggregatorHandler> readAggregators,
    std::unique_ptr<AggregatorHandler> writeAggregators,
    velocypack::Slice userParams) const -> std::unique_ptr<WorkerContext> {
  return std::make_unique<ShortestPathWorkerContext>(
      std::move(readAggregators), std::move(writeAggregators));
}

struct ShortestPathMasterContext : public MasterContext {
  ShortestPathMasterContext(uint64_t vertexCount, uint64_t edgeCount,
                            std::unique_ptr<AggregatorHandler> aggregators)
      : MasterContext(vertexCount, edgeCount, std::move(aggregators)){};
};
[[nodiscard]] auto ShortestPathAlgorithm::masterContext(
    std::unique_ptr<AggregatorHandler> aggregators,
    arangodb::velocypack::Slice userParams) const -> MasterContext* {
  return new ShortestPathMasterContext(0, 0, std::move(aggregators));
}
[[nodiscard]] auto ShortestPathAlgorithm::masterContextUnique(
    uint64_t vertexCount, uint64_t edgeCount,
    std::unique_ptr<AggregatorHandler> aggregators,
    arangodb::velocypack::Slice userParams) const
    -> std::unique_ptr<MasterContext> {
  return std::make_unique<ShortestPathMasterContext>(vertexCount, edgeCount,
                                                     std::move(aggregators));
}
