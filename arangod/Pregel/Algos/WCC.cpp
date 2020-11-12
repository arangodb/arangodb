////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
#include "Pregel/GraphStore.h"
#include "Pregel/IncomingCache.h"
#include "Pregel/VertexComputation.h"

#include "Logger/Logger.h"

#include <set>

using namespace arangodb;
using namespace arangodb::pregel;
using namespace arangodb::pregel::algos;

namespace {

struct WCCComputation : public VertexComputation<uint64_t, uint64_t, SenderMessage<uint64_t>> {
  WCCComputation() {}
  void compute(MessageIterator<SenderMessage<uint64_t>> const& messages) override {
    uint64_t currentComponent = vertexData();

    if (globalSuperstep() > 0) {  
      bool halt = true;

      for (const SenderMessage<uint64_t>* msg : messages) {
        if (msg->value < currentComponent) {
          currentComponent = msg->value;
          // TODO: optimization update the edge value if present
          // problem: there might be loads of edges, could be expensive
        }
      }
      
      SenderMessage<uint64_t> message(pregelId(), currentComponent);
      for (const SenderMessage<uint64_t>* msg : messages) {
        if (msg->value > currentComponent) {
          TRI_ASSERT(msg->senderId != pregelId());
          sendMessage(msg->senderId, message);
          halt = false;
        }
      }

      if (currentComponent != vertexData()) {
        *mutableVertexData() = currentComponent;
        halt = false;
      }
      
      if (halt) {
        voteHalt();
      } else {
        voteActive();
      }
    }
       
    if (this->getEdgeCount() > 0) {
      SenderMessage<uint64_t> message(pregelId(), currentComponent);    
      RangeIterator<Edge<uint64_t>> edges = this->getEdges();
      for (; edges.hasMore(); ++edges) {
        Edge<uint64_t>* edge = *edges;
        if (edge->toKey() == this->key()) {
          continue; // no need to send message to self
        }
      
        // remember the value we send
        edge->data() = currentComponent;

        sendMessage(edge, message);
      }
    }
  }
};

struct WCCGraphFormat final : public GraphFormat<uint64_t, uint64_t> {
  explicit WCCGraphFormat(application_features::ApplicationServer& server,
                         std::string const& result)
      : GraphFormat<uint64_t, uint64_t>(server), _resultField(result) {}
  
  std::string const _resultField;
  
  size_t estimatedVertexSize() const override { return sizeof(uint64_t); }
  size_t estimatedEdgeSize() const override { return sizeof(uint64_t); }

  void copyVertexData(std::string const& /*documentId*/, arangodb::velocypack::Slice /*document*/,
                      uint64_t& targetPtr, uint64_t& vertexIdRange) override {
    targetPtr = vertexIdRange++;
  }

  void copyEdgeData(arangodb::velocypack::Slice /*document*/, uint64_t& targetPtr) override {
    targetPtr = std::numeric_limits<uint64_t>::max();
  }

  bool buildVertexDocument(arangodb::velocypack::Builder& b, uint64_t const* ptr) const override {
    b.add(_resultField, arangodb::velocypack::Value(*ptr));
    return true;
  }
};
}

VertexComputation<uint64_t, uint64_t, SenderMessage<uint64_t>>* WCC::createComputation(
    WorkerConfig const* config) const {
  return new ::WCCComputation();
}

GraphFormat<uint64_t, uint64_t>* WCC::inputFormat() const {
  return new ::WCCGraphFormat(_server, _resultField);
}

