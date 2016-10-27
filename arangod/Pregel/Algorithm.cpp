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

#include "Algorithm.h"
#include "VertexComputation.h"

#include "GraphStore.h"
#include "IncomingCache.h"

using namespace arangodb;
using namespace arangodb::pregel;

size_t SSSPAlgorithm::estimatedVertexSize() const { return sizeof(int64_t); }

std::unique_ptr<GraphFormat<int64_t, int64_t>> SSSPAlgorithm::inputFormat()
    const {
  return std::make_unique<IntegerGraphFormat>("value", -1);
}

std::unique_ptr<MessageFormat<int64_t>> SSSPAlgorithm::messageFormat() const {
  return std::unique_ptr<IntegerMessageFormat>(new IntegerMessageFormat());
}

std::unique_ptr<MessageCombiner<int64_t>> SSSPAlgorithm::messageCombiner()
    const {
  return std::unique_ptr<MinIntegerCombiner>(new MinIntegerCombiner());
}

struct SSSPComputation : public VertexComputation<int64_t, int64_t, int64_t> {
  SSSPComputation() {}
  void compute(std::string const& vertexID,
               MessageIterator<int64_t> const& messages) override {
    int64_t tmp = *getVertexData();
    for (const int64_t* msg : messages) {
      if (tmp < 0 || *msg < tmp) {
        tmp = *msg;
      };
    }
    int64_t* state = getVertexData();
    if (tmp >= 0 && (getGlobalSuperstep() == 0 || tmp != *state)) {
      LOG(INFO) << "Recomputing value for vertex " << vertexID;
      *state = tmp;  // update state

      EdgeIterator<int64_t> edges = getEdges();
      for (EdgeEntry<int64_t>* edge : edges) {
        int64_t val = *edge->getData() + tmp;
        std::string const& toID = edge->toVertexID();
        sendMessage(toID, val);
      }
    }
    voteHalt();
  }
};

std::unique_ptr<VertexComputation<int64_t, int64_t, int64_t>>
SSSPAlgorithm::createComputation() const {
  return std::unique_ptr<SSSPComputation>(new SSSPComputation());
}
