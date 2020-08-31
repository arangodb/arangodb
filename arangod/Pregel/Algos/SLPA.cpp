////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#include "SLPA.h"
#include <atomic>
#include <cmath>
#include "Basics/StringUtils.h"
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

struct SLPAWorkerContext : public WorkerContext {
  uint32_t mod = 1;
  void preGlobalSuperstep(uint64_t gss) override {
    // lets switch the order randomly, but ensure equal listenting time
    if (gss % 2 == 0) {
      mod = RandomGenerator::interval(UINT32_MAX);
    }
  }
};

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
      val->memory.try_emplace(val->nodeId, 1);
      val->numCommunities = 1;
    }

    // Normally the SLPA algo only lets one vertex by one listen sequentially,
    // which is not really well parallizable. Additionally I figure
    // since a speaker only speaks to neighbours and the speaker order is random
    // we can get away with letting some nodes listen in turn
    SLPAWorkerContext const* ctx = reinterpret_cast<SLPAWorkerContext const*>(context());
    bool shouldListen = (ctx->mod + val->nodeId) % 2 == globalSuperstep() % 2;
    if (messages.size() > 0 && shouldListen) {
      // listen to our neighbours
      uint64_t newCommunity = mostFrequent(messages);
      auto it = val->memory.insert({newCommunity, 1});
      if (!it.second) {
        it.first->second++;
      }
      val->numCommunities++;
    }

    // speak to our neighbours
    uint64_t random = RandomGenerator::interval(val->numCommunities);
    uint64_t cumulativeSum = 0;
    // Randomly select a label with probability proportional to the
    // occurrence frequency of this label in its memory
    for (auto const& e : val->memory) {
      cumulativeSum += e.second;
      if (cumulativeSum >= random) {
        sendMessageToAllNeighbours(e.first);
        return;
      }
    }
    sendMessageToAllNeighbours(val->nodeId);
  }
};

VertexComputation<SLPAValue, int8_t, uint64_t>* SLPA::createComputation(WorkerConfig const* config) const {
  return new SLPAComputation();
}

struct SLPAGraphFormat : public GraphFormat<SLPAValue, int8_t> {
  std::string resField;
  uint64_t step = 1;
  double threshold;
  unsigned maxCommunities;

  explicit SLPAGraphFormat(application_features::ApplicationServer& server,
                           std::string const& result, double thr, unsigned mc)
      : GraphFormat<SLPAValue, int8_t>(server),
        resField(result),
        threshold(thr),
        maxCommunities(mc) {}

  size_t estimatedVertexSize() const override { return sizeof(LPValue); };
  size_t estimatedEdgeSize() const override { return 0; };

  void copyVertexData(std::string const& documentId, arangodb::velocypack::Slice document,
                        SLPAValue& value) override {
    value.nodeId = (uint32_t)_vertexIdRange++;
  }

  void copyEdgeData(arangodb::velocypack::Slice document, int8_t& targetPtr) override {
  }

  bool buildVertexDocument(arangodb::velocypack::Builder& b,
                           const SLPAValue* ptr, size_t size) const override {
    if (ptr->memory.empty()) {
      return false;
    } else {
      std::vector<std::pair<uint64_t, double>> vec;
      for (auto const& pair : ptr->memory) {
        double t = (double)pair.second / ptr->numCommunities;
        if (t >= threshold) {
          vec.emplace_back(pair.first, t);
        }
      }
      std::sort(vec.begin(), vec.end(),
                [](std::pair<uint64_t, double> a, std::pair<uint64_t, double> b) {
                  return a.second > b.second;
                });

      if (vec.empty()) {
        b.add(resField, VPackSlice::nullSlice());
      } else if (vec.size() == 1 || maxCommunities == 1) {
        b.add(resField, VPackValue(vec[0].first));
      } else {
        // output for use with the DMID/Metrics code
        b.add(resField, VPackValue(VPackValueType::Array));
        for (unsigned c = 0; c < vec.size() && c < maxCommunities; c++) {
          b.openArray();
          b.add(VPackValue(vec[c].first));
          b.add(VPackValue(vec[c].second));
          b.close();
        }
        b.close();
        /*b.add(resField, VPackValue(VPackValueType::Object));
         for (unsigned c = 0; c < vec.size() && c < maxCommunities; c++) {
         b.add(arangodb::basics::StringUtils::itoa(vec[c].first),
         VPackValue(vec[c].second));
         }
         b.close();*/
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
  return new SLPAGraphFormat(_server, _resultField, _threshold, _maxCommunities);
}

WorkerContext* SLPA::workerContext(velocypack::Slice userParams) const {
  return new SLPAWorkerContext();
};
