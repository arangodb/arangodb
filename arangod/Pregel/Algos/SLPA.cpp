////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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

#include "SLPA.h"
#include <atomic>
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

struct SLPAComputation : public VertexComputation<SLPAValue, int8_t, uint64_t> {
  SLPAComputation() {}

  uint64_t mostFrequent(MessageIterator<uint64_t> const& messages) {
    TRI_ASSERT(messages.size() > 0);
    if (messages.size() == 1) {
      return **messages;
    }

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
      return all[0];
    }
    return maxValue;
  }

  void compute(MessageIterator<uint64_t> const& messages) override {
    SLPAValue* val = mutableVertexData();
    if (globalSuperstep() == 0) {
      val->memory.emplace(val->nodeId, 1);
      val->numCommunities = 1;
    }

    // Normally the SLPA algo only lets one vertex by one listen sequentially,
    // which is not really well parallizable. Additionally I figure
    // since a speaker only speaks to neighbours and the speaker order is random
    // we can get away with letting nodes listen in turn
    bool listen = val->nodeId % 2 == globalSuperstep() % 2;
    if (messages.size() > 0 && listen) {
      // listen to our neighbours
      uint64_t newCommunity = mostFrequent(messages);
      auto it = val->memory.find(newCommunity);
      if (it == val->memory.end()) {
        val->memory.emplace(newCommunity, 1);
      } else {
        it->second++;
      }
      val->numCommunities++;
    }

    // speak to our neighbours
    uint64_t random = RandomGenerator::interval(val->numCommunities);
    uint64_t cumulativeSum = 0;
    // Randomly select a label with probability proportional to the
    // occurrence frequency of this label in its memory
    for (std::pair<uint64_t, uint64_t> const& e : val->memory) {
      cumulativeSum += e.second;
      if (cumulativeSum >= random) {
        sendMessageToAllNeighbours(e.first);
      }
    }
    sendMessageToAllNeighbours(val->nodeId);
  }
};

VertexComputation<SLPAValue, int8_t, uint64_t>* SLPA::createComputation(
    WorkerConfig const* config) const {
  return new SLPAComputation();
}

struct SLPAGraphFormat : public GraphFormat<SLPAValue, int8_t> {
  std::string resField;
  std::atomic<uint64_t> vertexIdRange;
  double threshold;
  unsigned maxCommunities;

  explicit SLPAGraphFormat(std::string const& result, double thr, unsigned mc)
      : resField(result),
        vertexIdRange(0),
        threshold(thr),
        maxCommunities(mc) {}

  size_t estimatedVertexSize() const override { return sizeof(LPValue); };
  size_t estimatedEdgeSize() const override { return 0; };

  void willLoadVertices(uint64_t count) override {
    // if we aren't running in a cluster it doesn't matter
    if (arangodb::ServerState::instance()->isRunningInCluster()) {
      arangodb::ClusterInfo* ci = arangodb::ClusterInfo::instance();
      if (ci) {  // get a counter range from the agency
        vertexIdRange = ci->uniqid(count);
      }
    }
  }

  size_t copyVertexData(std::string const& documentId,
                        arangodb::velocypack::Slice document, SLPAValue* value,
                        size_t maxSize) override {
    value->nodeId = (uint32_t)vertexIdRange++;
    return sizeof(SLPAValue);
  }

  size_t copyEdgeData(arangodb::velocypack::Slice document, int8_t* targetPtr,
                      size_t maxSize) override {
    return 0;
  }

  bool buildVertexDocument(arangodb::velocypack::Builder& b,
                           const SLPAValue* ptr, size_t size) const override {
    if (ptr->memory.empty()) {
      return false;
    } else {
      std::vector<uint64_t> communities;
      for (std::pair<uint64_t, uint64_t> pair : ptr->memory) {
        if ((double)pair.second / ptr->numCommunities >= threshold) {
          communities.push_back(pair.first);
        }
      }
      std::sort(communities.begin(), communities.end(),
                [ptr](uint64_t a, uint64_t b) {
                  return ptr->memory.at(a) > ptr->memory.at(b);
                });

      if (communities.empty()) {
        b.add(resField, VPackSlice::nullSlice());
      } else if (communities.size() == 1 || maxCommunities == 1) {
        b.add(resField, VPackValue(communities[0]));
      } else if (communities.size() > 1) {
        b.add(resField, VPackValue(VPackValueType::Array));
        for (unsigned c = 0; c < communities.size() && c < maxCommunities;
             c++) {
          b.add(VPackValue(communities[c]));
        }
        b.close();
      }
    }
    return true;
  }

  bool buildEdgeDocument(arangodb::velocypack::Builder& b, const int8_t* ptr,
                         size_t size) const override {
    return false;
  }
};

GraphFormat<SLPAValue, int8_t>* SLPA::inputFormat() const {
  return new SLPAGraphFormat(_resultField, _threshold, _maxCommunities);
}
