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
#include "Pregel/VertexComputation.h"
#include "Pregel/GraphStore.h"
#include "Pregel/IncomingCache.h"

using namespace arangodb::pregel;
using namespace arangodb::pregel::algos;

struct SSSPComputation : public VertexComputation<int64_t, int64_t, int64_t> {
    SSSPComputation() {}
    void compute(std::string const& vertexID,
                 MessageIterator<int64_t> const& messages) override {
        int64_t tmp = vertexData();
        for (const int64_t* msg : messages) {
            if (tmp < 0 || *msg < tmp) {
                tmp = *msg;
            };
        }
        int64_t* state = (int64_t*)mutableVertexData();
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

std::shared_ptr<GraphFormat<int64_t, int64_t>> SSSPAlgorithm::inputFormat()
    const {
  return std::make_shared<IntegerGraphFormat>("value", -1, 1);
}

std::shared_ptr<MessageFormat<int64_t>> SSSPAlgorithm::messageFormat() const {
  return std::shared_ptr<IntegerMessageFormat>(new IntegerMessageFormat());
}

std::shared_ptr<MessageCombiner<int64_t>> SSSPAlgorithm::messageCombiner()
    const {
  return std::shared_ptr<IntegerMinCombiner>(new IntegerMinCombiner());
}

std::shared_ptr<VertexComputation<int64_t, int64_t, int64_t>>
SSSPAlgorithm::createComputation() const {
  return std::shared_ptr<SSSPComputation>(new SSSPComputation());
}
