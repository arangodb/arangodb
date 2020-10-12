////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Pregel/Algorithm.h"
#include "Pregel/GraphStore.h"
#include "Pregel/IncomingCache.h"
#include "Pregel/VertexComputation.h"

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
        };
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
  explicit MyGraphFormat(application_features::ApplicationServer& server,
                         std::string const& result)
      : VertexGraphFormat<uint64_t, uint8_t>(server, result, /*vertexNull*/0) {}

  void copyVertexData(std::string const& documentId, arangodb::velocypack::Slice document,
                      uint64_t& targetPtr) override {
    targetPtr = _vertexIdRange++;
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

}

VertexComputation<uint64_t, uint8_t, uint64_t>* ConnectedComponents::createComputation(
    WorkerConfig const* config) const {
  return new MyComputation();
}

GraphFormat<uint64_t, uint8_t>* ConnectedComponents::inputFormat() const {
  return new MyGraphFormat(_server, _resultField);
}

VertexCompensation<uint64_t, uint8_t, uint64_t>* ConnectedComponents::createCompensation(
    WorkerConfig const* config) const {
  return new MyCompensation();
}
