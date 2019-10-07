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

#include "LineRank.h"
#include "Pregel/Aggregator.h"
#include "Pregel/GraphFormat.h"
#include "Pregel/Iterators.h"
#include "Pregel/MasterContext.h"
#include "Pregel/Utils.h"
#include "Pregel/VertexComputation.h"
#include "Pregel/WorkerContext.h"

using namespace arangodb;
using namespace arangodb::pregel;
using namespace arangodb::pregel::algos;

static std::string const kDiff = "diff";
static std::string const kLastIteration = "lastIteration";
static const float RESTART_PROB = 0.15f;
static const float EPS = 0.0000001f;

LineRank::LineRank(application_features::ApplicationServer& server,
                   arangodb::velocypack::Slice params)
    : SimpleAlgorithm(server, "LineRank", params) {
  // VPackSlice t = params.get("convergenceThreshold");
  //_threshold = t.isNumber() ? t.getNumber<float>() : 0.000002f;
}

struct LRMasterContext : MasterContext {
  bool _stopNext = false;
  bool postGlobalSuperstep() override {
    float const* diff = getAggregatedValue<float>(kDiff);
    TRI_ASSERT(!_stopNext || *diff == 0);
    if (_stopNext) {
      // return false;
      LOG_TOPIC("cc466", INFO, Logger::PREGEL) << "should stop " << globalSuperstep();
    } else if (globalSuperstep() > 0 && *diff < EPS) {
      aggregate<bool>(kLastIteration, true);
      _stopNext = true;
    }
    return true;
  };
};

/// do not recalculate startAtNodeProb in every compute call
struct LRWorkerContext : WorkerContext {
  float startAtNodeProb = 0;

  void preApplication() override { startAtNodeProb = 1.0f / edgeCount(); };
};

// github.com/JananiC/NetworkCentralities/blob/master/src/main/java/linerank/LineRank.java
struct LRComputation : public VertexComputation<float, float, float> {
  LRComputation() {}
  void compute(MessageIterator<float> const& messages) override {
    LRWorkerContext const* ctx = static_cast<LRWorkerContext const*>(context());

    float* vertexValue = mutableVertexData();
    if (localSuperstep() == 0) {
      *vertexValue = ctx->startAtNodeProb;
      sendMessageToAllNeighbours(*vertexValue);
    } else {
      float newScore = 0.0f;
      for (const float* msg : messages) {
        newScore += *msg;
      }

      bool const* lastIteration = getAggregatedValue<bool>(kLastIteration);
      if (*lastIteration) {
        *vertexValue = *vertexValue * getEdgeCount() + newScore;
        voteHalt();
      } else {
        if (getEdgeCount() == 0) {
          newScore = 0;
        } else {
          newScore /= getEdgeCount();
          newScore = ctx->startAtNodeProb * RESTART_PROB + newScore * (1.0f - RESTART_PROB);
        }

        float diff = fabsf(newScore - *vertexValue);
        *vertexValue = newScore;
        sendMessageToAllNeighbours(*vertexValue);

        aggregate<float>(kDiff, diff);
      }
    }
  }
};

VertexComputation<float, float, float>* LineRank::createComputation(WorkerConfig const* config) const {
  return new LRComputation();
}

WorkerContext* LineRank::workerContext(VPackSlice params) const {
  return new LRWorkerContext();
}

MasterContext* LineRank::masterContext(VPackSlice params) const {
  return new LRMasterContext();
}

IAggregator* LineRank::aggregator(std::string const& name) const {
  if (name == kLastIteration) {
    return new BoolOrAggregator(/* permanent: */ true);
  } else if (name == kDiff) {
    return new MaxAggregator<float>(false);  // non perm
  }
  return nullptr;
}
