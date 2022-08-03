////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Roman Rabinovich
////////////////////////////////////////////////////////////////////////////////

#include "ParameterizedPageRank.h"

#include <cmath>
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
// todo change to
//  constexpr std::string_view kConvergence{"convergence"};
static std::string const kConvergence{"convergence"};

struct PRWorkerContext : public WorkerContext {
  PRWorkerContext() = default;
};

ParameterizedPageRank::ParameterizedPageRank(
    application_features::ApplicationServer& server, VPackSlice const& params)
    : SimpleAlgorithm(server, "ParameterizedPageRank", params),
      _useSource(params.hasKey("sourceField")) {}

/// will use a seed value for ParameterizedPageRank if available
struct PPRGraphFormat final : public GraphFormat<PPRVertexData, PPREdgeData> {
  explicit PPRGraphFormat(application_features::ApplicationServer& server)
      : GraphFormat(server) {}

  // todo: complete
  // reads document which contains the complete document from the vertex
  // and saves it
  void copyVertexData(arangodb::velocypack::Options const& vpackOptions,
                      std::string const& documentId,
                      arangodb::velocypack::Slice document,
                      PPRVertexData& targetPtr,
                      uint64_t& vertexIdRange) override {}
  // todo complete
  // prepare builder to be written into document
  bool buildVertexDocument(arangodb::velocypack::Builder& b,
                           PPRVertexData const* targetPtr) const override {
    return true;
  }
};

GraphFormat<PPRVertexData, PPREdgeData>* ParameterizedPageRank::inputFormat()
    const {
  return new PPRGraphFormat(_server);
}

PPRVertexData computeNewValue(PPRVertexData oldValue,
                              MessageIterator<PPRMessageData> const& messages,
                              PRWorkerContext const* ctx, size_t gss) {
  if (gss == 0) {
    if (oldValue.value < 0) {
      // todo: compute it once
      // fixme: assure that if no vertices, we never reach this
      // initialize vertices to initial weight, unless there was a seed weight
      return PPRVertexData{1.0f / static_cast<float>(ctx->vertexCount())};
    }
    return PPRVertexData{0.0};
  } else {
    float sum = 0.0f;
    for (const PPRMessageData* msg : messages) {
      sum += msg->value;
    }
    // todo: compute it once
    // fixme: assure that if no vertices, we never reach this
    return PPRVertexData{0.85f * sum +
                         0.15f / static_cast<float>(ctx->vertexCount())};
  }
}

struct PPRComputation
    : public VertexComputation<PPRVertexData, PPREdgeData, PPRMessageData> {
  PPRComputation() = default;

  void compute(MessageIterator<PPRMessageData> const& messages) override {
    auto const* ctx = dynamic_cast<PRWorkerContext const*>(context());
    PPRVertexData* ptr = mutableVertexData();
    PPRVertexData copy = *ptr;

    *ptr = computeNewValue(copy, messages, ctx, globalSuperstep());
    float diff = fabs(copy.value - ptr->value);
    aggregate<float>(kConvergence, diff);

    size_t numEdges = getEdgeCount();
    if (numEdges > 0) {
      float val = ptr->value / numEdges;
      sendMessageToAllNeighbours(PPRMessageData{val});
    }
  }
};

VertexComputation<PPRVertexData, PPREdgeData, PPRMessageData>*
ParameterizedPageRank::createComputation(WorkerConfig const* config) const {
  return new PPRComputation();
}

WorkerContext* ParameterizedPageRank::workerContext(
    VPackSlice userParams) const {
  return new PRWorkerContext();
}

struct PRMasterContext : public MasterContext {
  float _threshold = EPS;
  explicit PRMasterContext(VPackSlice params) {
    VPackSlice t = params.get("threshold");
    _threshold = t.isNumber() ? t.getNumber<float>() : EPS;
  }

  void preApplication() override {
    LOG_TOPIC("e0598", DEBUG, Logger::PREGEL)
        << "Using threshold " << _threshold << " for ParameterizedPageRank";
  }

  bool postGlobalSuperstep() override {
    auto const diff =
        PPRVertexData{*getAggregatedValue<PPRVertexData>(kConvergence)};
    return globalSuperstep() < 1 || diff.value > _threshold;
  };
};

MasterContext* ParameterizedPageRank::masterContext(
    VPackSlice userParams) const {
  return new PRMasterContext(userParams);
}

IAggregator* ParameterizedPageRank::aggregator(std::string const& name) const {
  if (name == kConvergence) {
    return new MaxAggregator<float>(-1, false);
  }
  return nullptr;
}
