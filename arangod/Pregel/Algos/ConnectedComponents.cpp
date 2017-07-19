////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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

using namespace arangodb::pregel;
using namespace arangodb::pregel::algos;

struct MyComputation : public VertexComputation<int64_t, int64_t, int64_t> {
  MyComputation() {}
  void compute(MessageIterator<int64_t> const& messages) override {
    if (localSuperstep() == 0) {
      sendMessageToAllNeighbours(vertexData());
    } else {
      int64_t currentComponent = vertexData();
      for (const int64_t* msg : messages) {
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

VertexComputation<int64_t, int64_t, int64_t>*
ConnectedComponents::createComputation(WorkerConfig const* config) const {
  return new MyComputation();
}

struct MyGraphFormat : public VertexGraphFormat<int64_t, int64_t> {
  uint64_t vertexIdRange = 0;

  explicit MyGraphFormat(std::string const& result)
      : VertexGraphFormat<int64_t, int64_t>(result, 0) {}

  void willLoadVertices(uint64_t count) override {
    // if we aren't running in a cluster it doesn't matter
    if (arangodb::ServerState::instance()->isRunningInCluster()) {
      arangodb::ClusterInfo* ci = arangodb::ClusterInfo::instance();
      if (ci) {
        vertexIdRange = ci->uniqid(count);
      }
    }
  }

  size_t copyVertexData(std::string const& documentId,
                        arangodb::velocypack::Slice document,
                        int64_t* targetPtr, size_t maxSize) override {
    *targetPtr = vertexIdRange++;
    return sizeof(int64_t);
  }
};

GraphFormat<int64_t, int64_t>* ConnectedComponents::inputFormat() const {
  return new MyGraphFormat(_resultField);
}

struct MyCompensation : public VertexCompensation<int64_t, int64_t, int64_t> {
  MyCompensation() {}
  void compensate(bool inLostPartition) override {
    // actually don't do anything, graph format will reinitalize lost vertices

    /*if (inLostPartition) {
      int64_t* data = mutableVertexData();
      *data = INT64_MAX;
    }*/
  }
};

VertexCompensation<int64_t, int64_t, int64_t>*
ConnectedComponents::createCompensation(WorkerConfig const* config) const {
  return new MyCompensation();
}
