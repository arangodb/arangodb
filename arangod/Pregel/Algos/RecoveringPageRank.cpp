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

#include "RecoveringPageRank.h"
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

static std::string const kConvergence = "convergence";
static std::string const kStep = "step";
static std::string const kRank = "rank";
static std::string const kFailedCount = "failedCount";
static std::string const kNonFailedCount = "nonfailedCount";
static std::string const kScale = "scale";


RecoveringPageRank::RecoveringPageRank(arangodb::velocypack::Slice params)
    : SimpleAlgorithm("PageRank", params) {
  VPackSlice t = params.get("convergenceThreshold");
  _threshold = t.isNumber() ? t.getNumber<float>() : 0.000002f;
}

struct PageRankGraphFormat : public FloatGraphFormat {
  PageRankGraphFormat(std::string const& s, std::string const& r)
      : FloatGraphFormat(s, r, 0, 0) {}
  size_t copyVertexData(VertexEntry const& vertex,
                        std::string const& documentId,
                        arangodb::velocypack::Slice document, void* targetPtr,
                        size_t maxSize) override {
    *((float*)targetPtr) = _vDefault;
    return sizeof(float);
  }

  size_t copyEdgeData(arangodb::velocypack::Slice document, void* targetPtr,
                      size_t maxSize) override {
    return 0;
  }
};

GraphFormat<float, float>* RecoveringPageRank::inputFormat() {
  return new PageRankGraphFormat(_sourceField, _resultField);
}

MessageCombiner<float>* RecoveringPageRank::messageCombiner() const {
  return new FloatSumCombiner();
}

struct RPRComputation : public VertexComputation<float, float, float> {
  float _limit;
  RPRComputation(float t) : _limit(t) {}
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
    aggregate(kConvergence, &diff);
    aggregate(kRank, ptr);
    // const float* val = getAggregatedValue<float>("convergence");
    // if (val) {  // if global convergence is available use it
    //  diff = *val;
    //}

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

VertexComputation<float, float, float>* RecoveringPageRank::createComputation(
    WorkerConfig const* config) const {
  return new RPRComputation(_threshold);
}

Aggregator* RecoveringPageRank::aggregator(std::string const& name) const {
  if (name == kConvergence) {
    return new FloatMaxAggregator(-1);
  } else if (name == kNonFailedCount) {
    return new SumAggregator<uint32_t>(0);
  } else if (name == kRank) {
    return new SumAggregator<float>(0);
  } else if (name == kStep) {
    return new ValueAggregator<uint32_t>(0);
  } else if (name == kScale) {
    return new ValueAggregator<float>(-1);
  }
  return nullptr;
}

struct RPRCompensation : public VertexCompensation<float, float, float> {
  RPRCompensation() {}
  void compensate(bool inLostPartition) override {
    
    const uint32_t* step = getAggregatedValue<uint32_t>(kStep);
    if (*step == 0 && !inLostPartition) {
      uint32_t c = 1;
      aggregate(kNonFailedCount, &c);
      aggregate(kRank, mutableVertexData());
    } else if (*step == 1) {
      float* data = mutableVertexData();
      if (inLostPartition) {
        *data = 1.0f / context()->vertexCount();
      } else {
        const float* scale = getAggregatedValue<float>(kScale);
        if (*scale != 0) {
          *data *= *scale;
        }
      }
      
      voteActive();
    }
  }
};

VertexCompensation<float, float, float>* RecoveringPageRank::createCompensation(
    WorkerConfig const* config) const {
  return new RPRCompensation();
}

struct MyMasterContext : public MasterContext {
  MyMasterContext(VPackSlice params) : MasterContext(params){};

  int32_t recoveryStep = 0;
  float totalRank = 0;
  
  bool postGlobalSuperstep(uint64_t gss) override {
    const float* convergence = getAggregatedValue<float>(kConvergence);
    LOG(INFO) << "Current convergence level" << *convergence;
    totalRank = *getAggregatedValue<float>(kRank);
    return true;
  }

  bool preCompensation(uint64_t gss) override {
    aggregate(kStep, &recoveryStep);
    return totalRank != 0;
  }

  bool postCompensation(uint64_t gss) override {
    if (recoveryStep == 0) {
      recoveryStep = 1;
      
      const float* remainingRank = getAggregatedValue<float>(kRank);
      const uint32_t* nonfailedCount =
          getAggregatedValue<uint32_t>(kNonFailedCount);
      if (*remainingRank != 0 && *nonfailedCount != 0) {
        float scale = totalRank * (*nonfailedCount);
        scale /= this->vertexCount() * (*remainingRank);
        aggregate(kScale, &scale);
        return true;
      }
    }
    return false;
  }
};

MasterContext* RecoveringPageRank::masterContext(VPackSlice userParams) const {
  return new MyMasterContext(userParams);
}
