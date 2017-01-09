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

#include "SSSP.h"
#include "Pregel/Algorithm.h"
#include "Pregel/GraphStore.h"
#include "Pregel/IncomingCache.h"
#include "Pregel/VertexComputation.h"

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

      RangeIterator<Edge<int64_t>> edges = getEdges();
      for (Edge<int64_t>* edge : edges) {
        int64_t val = *edge->data() + tmp;
        sendMessage(edge, val);
      }
    }
    voteHalt();
  }
};

GraphFormat<int64_t, int64_t>* SSSPAlgorithm::inputFormat() {
  return new IntegerGraphFormat(_sourceField, _resultField, INT64_MAX, 1);
}

MessageFormat<int64_t>* SSSPAlgorithm::messageFormat() const {
  return new IntegerMessageFormat();
}

MessageCombiner<int64_t>* SSSPAlgorithm::messageCombiner() const {
  return new IntegerMinCombiner();
}

VertexComputation<int64_t, int64_t, int64_t>* SSSPAlgorithm::createComputation(
    WorkerConfig const* config) const {
  return new SSSPComputation();
}

struct SSSPCompensation : public VertexCompensation<int64_t, int64_t, int64_t> {
  SSSPCompensation() {}
  void compensate(bool inLostPartition) override {
    if (inLostPartition) {
      int64_t* data = mutableVertexData();
      *data = INT64_MAX;
    }
  }
};

VertexCompensation<int64_t, int64_t, int64_t>* SSSPAlgorithm::createCompensation(
                                                                               WorkerConfig const* config) const {
  return new SSSPCompensation();
}


