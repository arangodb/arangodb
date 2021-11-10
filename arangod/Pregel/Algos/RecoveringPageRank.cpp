////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
static std::string const kStep = "step";
static std::string const kRank = "rank";
static std::string const kFailedCount = "failedCount";
static std::string const kNonFailedCount = "nonfailedCount";
static std::string const kScale = "scale";

struct RPRComputation : public VertexComputation<float, float, float> {
  RPRComputation() {}
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
    aggregate(kConvergence, diff);
    aggregate(kRank, ptr);

    float val = *ptr / getEdgeCount();
    sendMessageToAllNeighbours(val);
  }
};

VertexComputation<float, float, float>* RecoveringPageRank::createComputation(
    WorkerConfig const* config) const {
  return new RPRComputation();
}

IAggregator* RecoveringPageRank::aggregator(std::string const& name) const {
  if (name == kConvergence) {
    return new MaxAggregator<float>(-1);
  } else if (name == kNonFailedCount) {
    return new SumAggregator<uint32_t>(0);
  } else if (name == kRank) {
    return new SumAggregator<float>(0);
  } else if (name == kStep) {
    return new OverwriteAggregator<uint32_t>(0);
  } else if (name == kScale) {
    return new OverwriteAggregator<float>(-1);
  }
  return nullptr;
}

struct RPRCompensation : public VertexCompensation<float, float, float> {
  RPRCompensation() {}
  void compensate(bool inLostPartition) override {
    auto const& step = getAggregatedValueRef<uint32_t>(kStep);
    if (step == 0 && !inLostPartition) {
      uint32_t c = 1;
      aggregate(kNonFailedCount, c);
      aggregate(kRank, mutableVertexData());
    } else if (step == 1) {
      float* data = mutableVertexData();
      if (inLostPartition) {
        *data = 1.0f / context()->vertexCount();
      } else {
        auto const& scale = &getAggregatedValueRef<float>(kScale);
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

struct RPRMasterContext : public MasterContext {
  float _threshold;

  explicit RPRMasterContext(VPackSlice params) {
    VPackSlice t = params.get("convergenceThreshold");
    _threshold = t.isNumber() ? t.getNumber<float>() : EPS;
  };

  int32_t recoveryStep = 0;
  float totalRank = 0;

  bool postGlobalSuperstep() override {
    const float* convergence = getAggregatedValue<float>(kConvergence);
    LOG_TOPIC("60fab", DEBUG, Logger::PREGEL) << "Current convergence level" << *convergence;
    totalRank = *getAggregatedValue<float>(kRank);

    float const* diff = getAggregatedValue<float>(kConvergence);
    return globalSuperstep() < 50 && *diff > _threshold;
  }

  bool preCompensation() override {
    aggregate(kStep, recoveryStep);
    return totalRank != 0;
  }

  bool postCompensation() override {
    if (recoveryStep == 0) {
      recoveryStep = 1;

      const float* remainingRank = getAggregatedValue<float>(kRank);
      const uint32_t* nonfailedCount = getAggregatedValue<uint32_t>(kNonFailedCount);
      if (*remainingRank != 0 && *nonfailedCount != 0) {
        float scale = totalRank * (*nonfailedCount);
        scale /= this->vertexCount() * (*remainingRank);
        aggregate(kScale, scale);
        return true;
      }
    }
    return false;
  }
};

MasterContext* RecoveringPageRank::masterContext(VPackSlice userParams) const {
  return new RPRMasterContext(userParams);
}
