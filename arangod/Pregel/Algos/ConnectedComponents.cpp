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
#include "Pregel/Algorithm.h"
#include "Pregel/GraphStore.h"
#include "Pregel/IncomingCache.h"
#include "Pregel/VertexComputation.h"

using namespace arangodb::pregel;
using namespace arangodb::pregel::algos;

struct MyComputation : public VertexComputation<int64_t, int64_t, int64_t> {
  MyComputation() {}
  void compute(MessageIterator<int64_t> const& messages) override {
    
    int64_t currentComponent = vertexData();
    for (const int64_t* msg : messages) {
      if (*msg < currentComponent) {
        currentComponent = *msg;
      };
    }
    
    if (currentComponent != vertexData()) {
      sendMessageToAllEdges(currentComponent);
    }
    voteHalt();
  }
};

VertexComputation<int64_t, int64_t, int64_t>* ConnectedComponents::createComputation(
    WorkerConfig const* config) const {
  return new MyComputation();
}

struct MyCompensation : public VertexCompensation<int64_t, int64_t, int64_t> {
  MyCompensation() {}
  void compensate(bool inLostPartition) override {
    if (inLostPartition) {
      int64_t* data = mutableVertexData();
      *data = INT64_MAX;
    }
  }
};

VertexCompensation<int64_t, int64_t, int64_t>*
ConnectedComponents::createCompensation(WorkerConfig const* config) const {
  return new MyCompensation();
}
