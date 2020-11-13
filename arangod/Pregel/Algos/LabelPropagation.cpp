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

#include "LabelPropagation.h"
#include <cmath>
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Pregel/Aggregator.h"
#include "Pregel/Algorithm.h"
#include "Pregel/GraphStore.h"
#include "Pregel/IncomingCache.h"
#include "Pregel/MasterContext.h"
#include "Pregel/VertexComputation.h"
#include "Random/RandomGenerator.h"

using namespace arangodb;
using namespace arangodb::pregel;
using namespace arangodb::pregel::algos;

static const uint64_t STABILISATION_ROUNDS = 20;

struct LPComputation : public VertexComputation<LPValue, int8_t, uint64_t> {
  LPComputation() {}

  uint64_t mostFrequent(MessageIterator<uint64_t> const& messages) {
    TRI_ASSERT(messages.size() > 0);

    // most frequent value
    size_t i = 0;
    std::vector<uint64_t> all(messages.size());
    for (uint64_t const* msg : messages) {
      all[i++] = *msg;
    }
    std::sort(all.begin(), all.end());
    uint64_t maxValue = all[0];
    uint64_t currentValue = all[0];
    int currentCounter = 1;
    int maxCounter = 1;
    for (i = 1; i < all.size(); i++) {
      if (currentValue == all[i]) {
        currentCounter++;
        if (maxCounter < currentCounter) {
          maxCounter = currentCounter;
          maxValue = currentValue;
        }
      } else {
        currentCounter = 1;
        currentValue = all[i];
      }
    }
    if (maxCounter == 1) {
      return std::min(all[0], mutableVertexData()->currentCommunity);
    }
    return maxValue;
  }

  void compute(MessageIterator<uint64_t> const& messages) override {
    LPValue* value = mutableVertexData();
    if (globalSuperstep() == 0) {
      sendMessageToAllNeighbours(value->currentCommunity);
    } else {
      uint64_t newCommunity = mutableVertexData()->currentCommunity;
      if (messages.size() == 1) {
        newCommunity = std::min(**messages, newCommunity);
      } else if (messages.size() > 1) {
        newCommunity = mostFrequent(messages);
      }

      // increment the stabilization count if vertex wants to stay in the
      // same partition
      if (value->lastCommunity == newCommunity) {
        value->stabilizationRounds++;
      }

      bool isUnstable = value->stabilizationRounds <= STABILISATION_ROUNDS;
      bool mayChange = value->currentCommunity != newCommunity;
      if (mayChange && isUnstable) {
        value->lastCommunity = value->currentCommunity;
        value->currentCommunity = newCommunity;
        value->stabilizationRounds = 0;  // reset stabilization counter
        sendMessageToAllNeighbours(value->currentCommunity);
      }
    }
    voteHalt();
  }
};

VertexComputation<LPValue, int8_t, uint64_t>* LabelPropagation::createComputation(
    WorkerConfig const* config) const {
  return new LPComputation();
}

struct LPGraphFormat : public GraphFormat<LPValue, int8_t> {
  std::string _resultField;

  explicit LPGraphFormat(application_features::ApplicationServer& server,
                         std::string const& result)
      : GraphFormat<LPValue, int8_t>(server), _resultField(result) {}

  size_t estimatedVertexSize() const override { return sizeof(LPValue); }
  size_t estimatedEdgeSize() const override { return 0; }

  void copyVertexData(std::string const& /*documentId*/, arangodb::velocypack::Slice /*document*/,
                      LPValue& value, uint64_t& vertexIdRange) override {
    value.currentCommunity = vertexIdRange++;
  }

  bool buildVertexDocument(arangodb::velocypack::Builder& b, LPValue const* ptr) const override {
    b.add(_resultField, VPackValue(ptr->currentCommunity));
    // b.add("stabilizationRounds", VPackValue(ptr->stabilizationRounds));
    return true;
  }
};

GraphFormat<LPValue, int8_t>* LabelPropagation::inputFormat() const {
  return new LPGraphFormat(_server, _resultField);
}
