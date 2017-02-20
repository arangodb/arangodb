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

#include "HITS.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Pregel/Aggregator.h"
#include "Pregel/Algorithm.h"
#include "Pregel/GraphStore.h"
#include "Pregel/IncomingCache.h"
#include "Pregel/MasterContext.h"
#include "Pregel/VertexComputation.h"

using namespace arangodb;
using namespace arangodb::pregel;
using namespace arangodb::pregel::algos;

static std::string const kAuthNorm = "auth";
static std::string const kHubNorm = "hub";

struct HITSComputation
    : public VertexComputation<HITSValue, int8_t, SenderMessage<float>> {
  HITSComputation() {}

  void compute(
      MessageIterator<SenderMessage<float>> const& messages) override {
    
    float auth = 0.0f;
    float hub = 0.0f;
    if (globalSuperstep() == 0) {
      auth = 1.0f / context()->vertexCount();
      hub = 1.0f / context()->vertexCount();
    } else {
      for (SenderMessage<float> const* message : messages) {
        // we don't put a valid shard id into the messages FROM our outgoing messages
        if (message->senderId.shard == invalid_prgl_shard) {
          hub += message->value;// auth from our outgoing Neighbors
        } else {
          auth += message->value;// hub from incoming Neighbors
        }
      }
      
      float const* authNorm = getAggregatedValue<float>(kAuthNorm);
      auth /= *authNorm;
      mutableVertexData()->authorityScore = auth;
      
      float const* hubNorm = getAggregatedValue<float>(kHubNorm);
      hub /= *hubNorm;
      mutableVertexData()->hubScore = hub;
    }
    
    // no sender required, the senders have an outgoing edge to us
    SenderMessage<float> authData(PregelID(invalid_prgl_shard, ""), auth);
    for (SenderMessage<float> const* message : messages) {
      if (message->senderId.shard != invalid_prgl_shard) {// send to incoming Neighbors
       sendMessage(message->senderId, authData);
      }
    }
    
    SenderMessage<float> hubData(this->pregelId(), hub);
    sendMessageToAllEdges(hubData);
    
    aggregate<float>(kAuthNorm, messages.size() > 0 ? auth * messages.size() : auth);
    auto const& edges = getEdges();
    aggregate<float>(kHubNorm, edges.size() > 0 ? hub * edges.size() : hub);
  }
};

VertexComputation<HITSValue, int8_t, SenderMessage<float>>*
HITS::createComputation(WorkerConfig const* config) const {
  return new HITSComputation();
}

struct HITSGraphFormat : public GraphFormat<HITSValue, int8_t> {
  const std::string _resultField;

  HITSGraphFormat(std::string const& result) : _resultField(result) {}

  size_t estimatedEdgeSize() const override { return 0; };

  size_t copyVertexData(std::string const& documentId,
                        arangodb::velocypack::Slice document,
                        HITSValue* targetPtr, size_t maxSize) override {
    return sizeof(HITSValue);
  }

  size_t copyEdgeData(arangodb::velocypack::Slice document, int8_t* targetPtr,
                      size_t maxSize) override {
    return 0;
  }

  bool buildVertexDocument(arangodb::velocypack::Builder& b,
                           const HITSValue* value, size_t size) const override {
    b.add(_resultField+"_auth", VPackValue(value->authorityScore));
    b.add(_resultField+"_hub", VPackValue(value->hubScore));
    return true;
  }

  bool buildEdgeDocument(arangodb::velocypack::Builder& b, const int8_t* ptr,
                         size_t size) const override {
    return false;
  }
};

GraphFormat<HITSValue, int8_t>* HITS::inputFormat() const {
  return new HITSGraphFormat(_resultField);
}
/*
struct HITSWorkerContext : public MasterContext {
  HITSWorkertContext() {}
  
  float authNormRoot;
  float hubNormRoor
  
};

WorkerContext* HITS::workerContext(VPackSlice userParams) const {
  
}*/

struct HITSMasterContext : public MasterContext {
  HITSMasterContext() {}  // TODO use _threashold
  
  bool postGlobalStep() {
    float const* authNorm = getAggregatedValue<float>(kAuthNorm);
    float const* hubNorm = getAggregatedValue<float>(kHubNorm);
    // will fail on small sparse graphs
    return *authNorm != 0 && *hubNorm != 0 && globalSuperstep() < 25;
  }
};

MasterContext* HITS::masterContext(VPackSlice userParams) const {
  return new HITSMasterContext();
}

IAggregator* HITS::aggregator(std::string const& name) const {
  if (name == kHubNorm || name == kAuthNorm) {
    return new SumAggregator<float>(false);// non perm
  }
  return nullptr;
}
