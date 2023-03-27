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

#include "ConnectedComponents.h"
#include "Cluster/ServerState.h"
#include "Pregel/Algorithm.h"
#include "Pregel/MasterContext.h"
#include "Pregel/IncomingCache.h"
#include "Pregel/VertexComputation.h"
#include "Pregel/WorkerContext.h"

using namespace arangodb;
using namespace arangodb::pregel;
using namespace arangodb::pregel::algos;

namespace {
struct MyComputation : public VertexComputation<uint64_t, uint8_t, uint64_t> {
  MyComputation() {}
  void compute(MessageIterator<uint64_t> const& messages) override {
    if (localSuperstep() == 0) {
      sendMessageToAllNeighbours(vertexData());
    } else {
      uint64_t currentComponent = vertexData();
      for (const uint64_t* msg : messages) {
        if (*msg < currentComponent) {
          currentComponent = *msg;
        }
      }

      if (currentComponent != vertexData()) {
        *mutableVertexData() = currentComponent;
        sendMessageToAllNeighbours(currentComponent);
      }
      voteHalt();
    }
  }
};

struct MyGraphFormat final : public VertexGraphFormat<uint64_t, uint8_t> {
  explicit MyGraphFormat(std::string const& result)
      : VertexGraphFormat<uint64_t, uint8_t>(result, /*vertexNull*/ 0) {}

  void copyVertexData(arangodb::velocypack::Options const&,
                      std::string const& /*documentId*/,
                      arangodb::velocypack::Slice /*document*/,
                      uint64_t& targetPtr, uint64_t vertexId) const override {
    targetPtr = vertexId;
  }
};

struct MyCompensation : public VertexCompensation<uint64_t, uint8_t, uint64_t> {
  MyCompensation() {}
  void compensate(bool inLostPartition) override {
    // actually don't do anything, graph format will reinitialize lost vertices

    /*if (inLostPartition) {
      int64_t* data = mutableVertexData();
      *data = INT64_MAX;
    }*/
  }
};

}  // namespace

VertexComputation<uint64_t, uint8_t, uint64_t>*
ConnectedComponents::createComputation(
    std::shared_ptr<WorkerConfig const> config) const {
  return new MyComputation();
}

std::shared_ptr<GraphFormat<uint64_t, uint8_t> const>
ConnectedComponents::inputFormat() const {
  return std::make_shared<MyGraphFormat>(_resultField);
}

VertexCompensation<uint64_t, uint8_t, uint64_t>*
ConnectedComponents::createCompensation(
    std::shared_ptr<WorkerConfig const> config) const {
  return new MyCompensation();
}

struct ConnectedComponentsWorkerContext : public WorkerContext {
  ConnectedComponentsWorkerContext(
      std::unique_ptr<AggregatorHandler> readAggregators,
      std::unique_ptr<AggregatorHandler> writeAggregators)
      : WorkerContext(std::move(readAggregators),
                      std::move(writeAggregators)){};
};
[[nodiscard]] auto ConnectedComponents::workerContext(
    std::unique_ptr<AggregatorHandler> readAggregators,
    std::unique_ptr<AggregatorHandler> writeAggregators,
    velocypack::Slice userParams) const -> WorkerContext* {
  return new ConnectedComponentsWorkerContext(std::move(readAggregators),
                                              std::move(writeAggregators));
}
[[nodiscard]] auto ConnectedComponents::workerContextUnique(
    std::unique_ptr<AggregatorHandler> readAggregators,
    std::unique_ptr<AggregatorHandler> writeAggregators,
    velocypack::Slice userParams) const -> std::unique_ptr<WorkerContext> {
  return std::make_unique<ConnectedComponentsWorkerContext>(
      std::move(readAggregators), std::move(writeAggregators));
}

struct ConnectedComponentsMasterContext : public MasterContext {
  ConnectedComponentsMasterContext(
      uint64_t vertexCount, uint64_t edgeCount,
      std::unique_ptr<AggregatorHandler> aggregators)
      : MasterContext(vertexCount, edgeCount, std::move(aggregators)){};
};
[[nodiscard]] auto ConnectedComponents::masterContext(
    std::unique_ptr<AggregatorHandler> aggregators,
    arangodb::velocypack::Slice userParams) const -> MasterContext* {
  return new ConnectedComponentsMasterContext(0, 0, std::move(aggregators));
}
[[nodiscard]] auto ConnectedComponents::masterContextUnique(
    uint64_t vertexCount, uint64_t edgeCount,
    std::unique_ptr<AggregatorHandler> aggregators,
    arangodb::velocypack::Slice userParams) const
    -> std::unique_ptr<MasterContext> {
  return std::make_unique<ConnectedComponentsMasterContext>(
      vertexCount, edgeCount, std::move(aggregators));
}
