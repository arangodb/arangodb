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

#include "Pregel/Algos/ShortestPath.h"
#include "Pregel/Aggregator.h"
#include "Pregel/Algorithm.h"
#include "Pregel/GraphStore.h"
#include "Pregel/IncomingCache.h"
#include "Pregel/VertexComputation.h"
#include "Pregel/WorkerConfig.h"

using namespace arangodb::pregel;
using namespace arangodb::pregel::algos;

static std::string const spUpperPathBound = "bound";

struct ShortestPathComp : public VertexComputation<int64_t, int64_t, int64_t> {
  PregelID _target;

  ShortestPathComp(PregelID const& target) : _target(target) {}
  void compute(MessageIterator<int64_t> const& messages) override {
    int64_t current = vertexData();
    for (const int64_t* msg : messages) {
      if (*msg < current) {
        current = *msg;
      };
    }
    
    // use global state to limit the computation of paths
    bool isSource = current == 0 && localSuperstep() == 0;
    int64_t const* max = getAggregatedValue<int64_t>(spUpperPathBound);

    int64_t* state = mutableVertexData();
    if (isSource || (current < *state && current < *max)) {
      *state = current;  // update state

      if (this->pregelId() == _target) {
        // TODO extend pregel to update certain aggregators during a GSS
        aggregate(spUpperPathBound, current);
        enterNextPhase();
        return;
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

struct arangodb::pregel::algos::SPGraphFormat
    : public GraphFormat<int64_t, int64_t> {
  std::string _sourceDocId, _targetDocId;

 public:
  SPGraphFormat(std::string const& source, std::string const& target)
      : _sourceDocId(source), _targetDocId(target) {}

  size_t copyVertexData(VertexEntry const& vertex,
                        std::string const& documentId,
                        arangodb::velocypack::Slice document, void* targetPtr,
                        size_t maxSize) override {
    *((int64_t*)targetPtr) = documentId == _sourceDocId ? 0 : INT64_MAX;
    return sizeof(int64_t);
  }

  size_t copyEdgeData(arangodb::velocypack::Slice document, void* targetPtr,
                      size_t maxSize) override {
    *((int64_t*)targetPtr) = 1;
    return sizeof(int64_t);
  }

  bool buildVertexDocument(arangodb::velocypack::Builder& b,
                           const void* targetPtr, size_t size) override {
    int64_t* val = (int64_t*)targetPtr;
    if (*val != INT64_MAX) {
      b.add("length", VPackValue(*val));
      return true;
    }
    return false;
  }

  bool buildEdgeDocument(arangodb::velocypack::Builder& b,
                         const void* targetPtr, size_t size) override {
    return false;
  }
};

ShortestPathAlgorithm::ShortestPathAlgorithm(VPackSlice userParams)
    : Algorithm("ShortestPath") {
  VPackSlice val1 = userParams.get("source");
  VPackSlice val2 = userParams.get("target");
  if (val1.isNone() || val2.isNone()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "You must specify source and target");
  }
  _format = new SPGraphFormat(val1.copyString(), val2.copyString());
}

std::vector<std::string> ShortestPathAlgorithm::initialActiveSet() {
  return std::vector<std::string>{_format->_sourceDocId};
}

GraphFormat<int64_t, int64_t>* ShortestPathAlgorithm::inputFormat() {
  return _format;
}

VertexComputation<int64_t, int64_t, int64_t>*
ShortestPathAlgorithm::createComputation(WorkerConfig const* _config) const {
  PregelID target = _config->documentIdToPregel(_format->_targetDocId);
  return new ShortestPathComp(target);
}

Aggregator* ShortestPathAlgorithm::aggregator(std::string const& name) const {
  if (name == spUpperPathBound) {  // persistent min operator
    return new MinAggregator<int64_t>(INT64_MAX, true);
  }
  return nullptr;
}
