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

#include "ShortestPath.h"
#include "Pregel/Algorithm.h"
#include "Pregel/Aggregator.h"
#include "Pregel/GraphStore.h"
#include "Pregel/IncomingCache.h"
#include "Pregel/VertexComputation.h"
#include "Pregel/WorkerConfig.h"

using namespace arangodb::pregel;
using namespace arangodb::pregel::algos;

static std::string const spUpperPathBound = "bound";

struct ShortestPathComp : public VertexComputation<int64_t, int64_t, int64_t> {
  PregelKey _target;
  
  
  ShortestPathComp(PregelKey const& target) : _target(target) {}
  void compute(MessageIterator<int64_t> const& messages) override {
    
    int64_t const* max = getAggregatedValue<int64_t>(spUpperPathBound);
    int64_t current = vertexData();
    for (const int64_t* msg : messages) {
      if (*msg < current) {
        current = *msg;
      };
    }
    
    int64_t* state = mutableVertexData();
    if (current < *max && current < *state) {
      *state = current;  // update state
      
      if (isDocument(_targetDocumentId)) {
        
        aggregate(spUpperPathBound, &val);
      }
      
      RangeIterator<Edge<int64_t>> edges = getEdges();
      for (Edge<int64_t>* edge : edges) {
        int64_t val = *edge->data() + current;
        if (val < *max) {
          sendMessage(edge, val);
        }
      }
    }
    
    voteHalt();
  }
};

struct SPGraphFormat : public GraphFormat<int64_t, int64_t> {
  std::string _sourceDocId, _targetDocId;
  
public:
  SPGraphFormat(std::string const& source, std::string const& target)
  : _sourceDocId(source), _targetDocId(target) {}
  
  size_t copyVertexData(std::string const& documentId,
                        arangodb::velocypack::Slice document,
                        void* targetPtr, size_t maxSize) override {
    //arangodb::velocypack::Slice val = document.get(_sourceField);
    // = val.isInteger() ? val.getInt() : _vDefault;
    *((int64_t*)targetPtr) = (documentId == _sourceDocId) ? 0 : INT64_MAX;
    return sizeof(int64_t);
  }
  
  size_t copyEdgeData(arangodb::velocypack::Slice document,
                      void* targetPtr, size_t maxSize) override {
    *((int64_t*)targetPtr) = 1;
    return sizeof(int64_t);
  }
  
  void buildVertexDocument(arangodb::velocypack::Builder& b,
                           const void* targetPtr, size_t size) override {
    //b.add(_resultField, VPackValue(readVertexData(targetPtr)));
  }
  
  void buildEdgeDocument(arangodb::velocypack::Builder& b,
                         const void* targetPtr, size_t size) override {
    //b.add(_resultField, VPackValue(*((int64_t*)targetPtr)));
  }
};

ShortestPathAlgorithm::ShortestPathAlgorithm(VPackSlice userParams) : Algorithm("ShortestPath") {
  _sourceDocId = userParams.get("source").copyString();
  _targetDocId = userParams.get("target").copyString();
}

GraphFormat<int64_t, int64_t>* ShortestPathAlgorithm::inputFormat()
    const {
  return new SPGraphFormat(_sourceDocId, _targetDocId);
}

MessageFormat<int64_t>* ShortestPathAlgorithm::messageFormat() const {
  return new IntegerMessageFormat();
}

MessageCombiner<int64_t>* ShortestPathAlgorithm::messageCombiner()
    const {
  return new IntegerMinCombiner();
}

VertexComputation<int64_t, int64_t, int64_t>*
ShortestPathAlgorithm::createComputation(WorkerConfig const*_config) const {
  return new ShortestPathComp(_targetDocId);
}

Aggregator* ShortestPathAlgorithm::aggregator(std::string const& name) const {
  if (name == spUpperPathBound) {
    return new MinAggregator<int64_t>(INT64_MAX);
  }
  return nullptr;
}
