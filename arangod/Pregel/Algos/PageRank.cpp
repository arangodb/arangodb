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
#include "Pregel/GraphFormat.h"
#include "Pregel/Iterators.h"
#include "Pregel/MasterContext.h"
#include "Pregel/Utils.h"
#include "Pregel/VertexComputation.h"

using namespace arangodb;
using namespace arangodb::pregel;
using namespace arangodb::pregel::algos;

static float EPS = 0.00001f;
static std::string const kConvergence = "convergence";

struct PRWorkerContext : public WorkerContext {
  PRWorkerContext() {}

  float commonProb = 0;
  void preGlobalSuperstep(uint64_t gss) override {
    if (gss == 0) {
      commonProb = 1.0f / vertexCount();
    } else {
      commonProb = 0.15f / vertexCount();
    }
  }
};

PageRank::PageRank(VPackSlice const& params)
    : SimpleAlgorithm("PageRank", params) {
  _maxGSS = basics::VelocyPackHelper::getNumericValue(params, "maxIterations", 250);
}

struct PRComputation : public VertexComputation<float, float, float> {
  PRComputation() {}
  void compute(MessageIterator<float> const& messages) override {
    PRWorkerContext const* ctx = static_cast<PRWorkerContext const*>(context());
    float* ptr = mutableVertexData();
    float copy = *ptr;

    if (globalSuperstep() == 0) {
      *ptr = ctx->commonProb;
    } else {
      float sum = 0.0f;
      for (const float* msg : messages) {
        sum += *msg;
      }
      *ptr = 0.85f * sum + ctx->commonProb;
    }
    float diff = fabs(copy - *ptr);
    aggregate<float>(kConvergence, diff);

    float val = *ptr / getEdgeCount();
    sendMessageToAllNeighbours(val);
  }
};

VertexComputation<float, float, float>* PageRank::createComputation(WorkerConfig const* config) const {
  return new PRComputation();
}

WorkerContext* PageRank::workerContext(VPackSlice userParams) const {
  return new PRWorkerContext();
}

struct PRMasterContext : public MasterContext {
  float _threshold = EPS;
  explicit PRMasterContext(VPackSlice params) {
    VPackSlice t = params.get("threshold");
    _threshold = t.isNumber() ? t.getNumber<float>() : EPS;
  }

  void preApplication() override {
    LOG_TOPIC(DEBUG, Logger::PREGEL) << "Using threshold " << _threshold;
  };

  bool postGlobalSuperstep() override {
    float const* diff = getAggregatedValue<float>(kConvergence);
    return globalSuperstep() < 1 || *diff > _threshold;
  };
};

MasterContext* PageRank::masterContext(VPackSlice userParams) const {
  return new PRMasterContext(userParams);
}

IAggregator* PageRank::aggregator(std::string const& name) const {
  if (name == kConvergence) {
    return new MaxAggregator<float>(-1, false);
  }
  return nullptr;
}
