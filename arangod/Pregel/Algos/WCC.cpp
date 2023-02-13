////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#include "WCC.h"

#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Pregel/Algorithm.h"
#include "Pregel/Worker/GraphStore.h"
#include "Pregel/IncomingCache.h"
#include "Pregel/VertexComputation.h"

#include "Logger/Logger.h"

#include <set>

using namespace arangodb;
using namespace arangodb::pregel;
using namespace arangodb::pregel::algos;

namespace {

struct WCCComputation
    : public VertexComputation<WCCValue, uint64_t, SenderMessage<uint64_t>> {
  WCCComputation() {}
  void compute(
      MessageIterator<SenderMessage<uint64_t>> const& messages) override {
    bool shouldPropagate = selectMinimumOfLocalAndInput(messages);
    // We need to propagate on first step
    TRI_ASSERT(globalSuperstep() != 0 || shouldPropagate);

    if (shouldPropagate) {
      propagate();
    }
    // We can always stop.
    // Every vertex will be awoken on
    // input messages. If there are no input
    // messages for us, we have the same ID
    // as our neighbors.
    voteHalt();
  }

 private:
  /**
   * @brief Scan the input, compare it pairwise with our current value.
   * We store the minimum into our current value.
   * And return true, whenever there was a difference between input and our
   * value. This difference indicates that the sender or this vertex are in
   * different components if this vertex is off, we will send the new component
   * to all our neighbors, if the other vertex is off, we will send our
   * component back. Will always return true in the very first step, as this
   * kicks of the algorithm and does not yet have input.
   */
  bool selectMinimumOfLocalAndInput(
      MessageIterator<SenderMessage<uint64_t>> const& messages) {
    // On first iteration, we need to propagate.
    // Otherwise the default is to stay silent, unless some message
    // sends a different component then us.
    // Either the sender has a wrong component or we have.
    bool shouldPropagate = globalSuperstep() == 0;

    auto& myData = *mutableVertexData();
    for (const SenderMessage<uint64_t>* msg : messages) {
      if (globalSuperstep() == 1) {
        // In the first step, we need to retain all inbound connections
        // for propagation
        myData.inboundNeighbors.emplace(msg->senderId);
      }
      if (msg->value != myData.component) {
        // we have a difference. Send updates
        shouldPropagate = true;
        if (msg->value < myData.component) {
          // The other component is lower.
          // We join this component
          myData.component = msg->value;
        }
      }
    }
    return shouldPropagate;
  }

  /**
   * @brief
   * Send the current vertex data to all our neighbors, inbound
   * and outbound.
   * Store the component value in the outbound edges
   */
  void propagate() {
    auto const& myData = vertexData();
    SenderMessage<uint64_t> message(pregelId(), myData.component);
    // Send to OUTBOUND neighbors
    RangeIterator<Edge<uint64_t>> edges = this->getEdges();
    for (; edges.hasMore(); ++edges) {
      Edge<uint64_t>* edge = *edges;
      if (edge->toKey() == this->key()) {
        continue;  // no need to send message to self
      }

      // remember the value we send
      // NOTE: I have done refactroing of the algorithm
      // the original variant saved this, i do not know
      // if it is actually relevant for anything.
      edge->data() = myData.component;

      sendMessage(edge, message);
    }
    // Also send to INBOUND neighbors
    for (auto const& target : myData.inboundNeighbors) {
      sendMessage(target, message);
    }
  }

 private:
};

struct WCCGraphFormat final : public GraphFormat<WCCValue, uint64_t> {
  explicit WCCGraphFormat(application_features::ApplicationServer& server,
                          std::string const& result)
      : GraphFormat<WCCValue, uint64_t>(server), _resultField(result) {}

  std::string const _resultField;

  size_t estimatedVertexSize() const override {
    // This is a very rough and guessed estimate.
    // We need some space for the inbound connections,
    // but we have not a single clue how many we will have
    return sizeof(uint64_t) + 8 * sizeof(VertexID);
  }
  size_t estimatedEdgeSize() const override { return sizeof(uint64_t); }

  void copyVertexData(arangodb::velocypack::Options const&,
                      std::string const& /*documentId*/,
                      arangodb::velocypack::Slice /*document*/,
                      WCCValue& targetPtr, uint64_t& vertexIdRange) override {
    targetPtr.component = vertexIdRange++;
  }

  void copyEdgeData(arangodb::velocypack::Options const&,
                    arangodb::velocypack::Slice /*document*/,
                    uint64_t& targetPtr) override {
    targetPtr = std::numeric_limits<uint64_t>::max();
  }

  bool buildVertexDocument(arangodb::velocypack::Builder& b,
                           WCCValue const* ptr) const override {
    b.add(_resultField, arangodb::velocypack::Value(ptr->component));
    return true;
  }
};
}  // namespace

VertexComputation<WCCValue, uint64_t, SenderMessage<uint64_t>>*
WCC::createComputation(WorkerConfig const* config) const {
  return new ::WCCComputation();
}

GraphFormat<WCCValue, uint64_t>* WCC::inputFormat() const {
  return new ::WCCGraphFormat(_server, _resultField);
}
