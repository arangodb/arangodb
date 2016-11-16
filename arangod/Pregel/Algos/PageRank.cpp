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

#include "PageRank.h"
#include "Pregel/Aggregator.h"
#include "Pregel/Combiners/FloatSumCombiner.h"
#include "Pregel/GraphFormat.h"
#include "Pregel/MessageIterator.h"
#include "Pregel/Utils.h"
#include "Pregel/VertexComputation.h"

#include "Cluster/ClusterInfo.h"
#include "Utils/OperationCursor.h"
#include "Utils/SingleCollectionTransaction.h"
#include "Utils/StandaloneTransactionContext.h"
#include "Utils/Transaction.h"
#include "Vocbase/vocbase.h"

using namespace arangodb;
using namespace arangodb::pregel;
using namespace arangodb::pregel::algos;

PageRankAlgorithm::PageRankAlgorithm(arangodb::velocypack::Slice params) : SimpleAlgorithm("PageRank", params) {
  VPackSlice t = params.get("convergenceThreshold");
  _threshold = t.isNumber() ? t.getNumber<float>() : 0.00002;
}

struct PageRankGraphFormat : public FloatGraphFormat {
  PageRankGraphFormat(std::string const& s, std::string const& r)
      : FloatGraphFormat(s, r, 0, 0) {}
  bool storesEdgeData() const override { return false; }
  size_t copyVertexData(arangodb::velocypack::Slice document, void* targetPtr,
                        size_t maxSize) override {
    *((float*)targetPtr) = _vDefault;
    return sizeof(float);
  }
};

GraphFormat<float, float>* PageRankAlgorithm::inputFormat()
    const {
  return new PageRankGraphFormat(_sourceField, _resultField);
}

MessageFormat<float>* PageRankAlgorithm::messageFormat() const {
  return new FloatMessageFormat();
}

MessageCombiner<float>* PageRankAlgorithm::messageCombiner()
    const {
  return new FloatSumCombiner();
}

struct PageRankComputation : public VertexComputation<float, float, float> {
  float _limit;
  PageRankComputation(float t) : _limit(t) {}
  void compute(std::string const& vertexID,
               MessageIterator<float> const& messages) override {
    
    float* ptr = mutableVertexData<float>();
    float copy = *ptr;
    // TODO do initialization in GraphFormat?
    if (globalSuperstep() == 0 && *ptr == 0) {
      *ptr = 1.0 / context()->vertexCount();
    } else if (globalSuperstep() > 0) {
      float sum = 0;
      for (const float* msg : messages) {
        sum += *msg;
      }
      *ptr = 0.15 / context()->vertexCount() + 0.85 * sum;
    }
    float diff = fabsf(copy - *ptr);
    aggregate("convergence", &diff);
    const float *val = getAggregatedValue<float>("convergence");
    if (val) {// if global convergence is available use it
      diff = *val;
    }
    
    if (globalSuperstep() < 50 && diff > _limit) {
      RangeIterator<EdgeEntry<float>> edges = getEdges();
      float val = *ptr / edges.size();
      for (EdgeEntry<float>* edge : edges) {
        sendMessage(edge->toVertexID(), val);
      }
    } else {
      voteHalt();
    }
  }
};

VertexComputation<float, float, float>*
PageRankAlgorithm::createComputation(uint64_t gss) const {
  return new PageRankComputation(_threshold);
}

Aggregator* PageRankAlgorithm::aggregator(std::string const& name) const {
  if (name == "convergence") {
    return new FloatMaxAggregator(0);
  }
  return nullptr;
}
