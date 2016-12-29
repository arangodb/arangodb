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
#include "Pregel/Iterators.h"
#include "Pregel/MasterContext.h"
#include "Pregel/Utils.h"
#include "Pregel/VertexComputation.h"

#include "Cluster/ClusterInfo.h"
#include "Utils/OperationCursor.h"
#include "Utils/SingleCollectionTransaction.h"
#include "Utils/StandaloneTransactionContext.h"
#include "Utils/Transaction.h"
#include "VocBase/vocbase.h"

using namespace arangodb;
using namespace arangodb::pregel;
using namespace arangodb::pregel::algos;

PageRankAlgorithm::PageRankAlgorithm(arangodb::velocypack::Slice params) : SimpleAlgorithm("PageRank", params) {
  VPackSlice t = params.get("convergenceThreshold");
  _threshold = t.isNumber() ? t.getNumber<float>() : 0.00002f;
}

struct PageRankGraphFormat : public FloatGraphFormat {
  PageRankGraphFormat(std::string const& s, std::string const& r)
      : FloatGraphFormat(s, r, 0, 0) {}
  bool storesEdgeData() const override { return false; }
  size_t copyVertexData(std::string const& documentId,
                        arangodb::velocypack::Slice document,
                        void* targetPtr, size_t maxSize) override {
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

struct PRComputation : public VertexComputation<float, float, float> {
  float _limit;
  PRComputation(float t) : _limit(t) {}
  void compute(MessageIterator<float> const& messages) override {
    
    float* ptr = mutableVertexData();
    float copy = *ptr;
    // TODO do initialization in GraphFormat?
    if (globalSuperstep() == 0 && *ptr == 0) {
      *ptr = 1.0f / context()->vertexCount();
    } else if (globalSuperstep() > 0) {
      float sum = 0;
      for (const float* msg : messages) {
        sum += *msg;
      }
      *ptr = 0.15f / context()->vertexCount() + 0.85f * sum;
    }
    float diff = fabsf(copy - *ptr);
    aggregate("convergence", &diff);
    const float *val = getAggregatedValue<float>("convergence");
    if (val) {// if global convergence is available use it
      diff = *val;
    }
    
    if (globalSuperstep() < 50 && diff > _limit) {
      RangeIterator<Edge<float>> edges = getEdges();
      float val = *ptr / edges.size();
      for (Edge<float>* edge : edges) {
        sendMessage(edge, val);
      }
    } else {
      voteHalt();
    }
  }
};

VertexComputation<float, float, float>*
PageRankAlgorithm::createComputation(WorkerConfig const* config) const {
  return new PRComputation(_threshold);
}

struct PRCompensation : public VertexCompensation<float, float, float> {
  
  PRCompensation() {}
  void compensate(bool inLostPartition) override {
    
    const uint32_t *step = getAggregatedValue<uint32_t>("step");
    if (step) {
      if (*step == 0 && !inLostPartition) {
        uint32_t c = 1;
        aggregate("nonfailedCount", &c);
        aggregate("totalrank", mutableVertexData());
      } else if (*step == 1) {
        
        float *data = mutableVertexData();
        if (inLostPartition) {
          *data = 1.0f / context()->vertexCount();
        } else {
          const float *scale = getAggregatedValue<float>("scale");
          if (scale && *scale != 0) {
            *data *= *scale;
          }
        }
        
      }
    }
  }
};

VertexCompensation<float, float, float>*
PageRankAlgorithm::createCompensation(WorkerConfig const* config) const {
  return new PRCompensation();
}

Aggregator* PageRankAlgorithm::aggregator(std::string const& name) const {
  if (name == "convergence") {
    return new FloatMaxAggregator(-1);
  } else if (name == "nonfailedCount") {
    return new SumAggregator<uint32_t>(0);
  } else if (name == "totalrank") {
    return new SumAggregator<float>(0);
  } else if (name == "step") {
    return new ValueAggregator<uint32_t>(0);
  } else if (name == "scale") {
    return new ValueAggregator<float>(-1);
  }
  return nullptr;
}

struct PRMasterContext : public MasterContext {
  PRMasterContext(VPackSlice params) : MasterContext(params) {};
  bool postGlobalSuperstep(uint64_t gss) {
    const float *convergence = getAggregatedValue<float>("convergence");
    LOG(INFO) << "Current convergence level" << *convergence;
    return true;
  }
  
  int32_t recoveryStep = -1;
  
  bool preCompensation(uint64_t gss) {
    recoveryStep++;
    aggregate("step", &recoveryStep);
    return true;
  }
  
  bool postCompensation(uint64_t gss) {
    if (recoveryStep == 0) {
      const float* totalrank = getAggregatedValue<float>("totalrank");
      const uint32_t* nonfailedCount = getAggregatedValue<uint32_t>("nonfailedCount");
      if (totalrank && *totalrank != 0 && nonfailedCount && *nonfailedCount != 0) {
        float scale = ((*nonfailedCount) * 1.0f) / (this->vertexCount() * (*totalrank));
        aggregate("scale", &scale);
        return true;
      }
    }
    recoveryStep = -1;
    return false;
  }
};


MasterContext* PageRankAlgorithm::masterContext(VPackSlice userParams) const {
  return new PRMasterContext(userParams);
}
