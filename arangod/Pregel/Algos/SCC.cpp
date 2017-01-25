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

#include "SCC.h"
#include "Pregel/Aggregator.h"
#include "Pregel/Algorithm.h"
#include "Pregel/GraphStore.h"
#include "Pregel/IncomingCache.h"
#include "Pregel/VertexComputation.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Pregel/MasterContext.h"

using namespace arangodb::pregel;
using namespace arangodb::pregel::algos;

static std::string const kPhase = "phase";
static std::string const kFoundNewMax = "max";
static std::string const kConverged = "converged";

enum SCCPhase {
  TRANSPOSE = 0,
  TRIMMING = 1,
  FORWARD_TRAVERSAL = 2,
  BACKWARD_TRAVERSAL_START = 3,
  BACKWARD_TRAVERSAL_REST = 4
};

struct SCCComputation : public VertexComputation<SCCValue, int32_t, SenderMessage<uint64_t>> {
  SCCComputation() {}

  void compute(MessageIterator<SenderMessage<uint64_t>> const& messages) override {
    if (isActive() == false) {
      // color was already determinded or vertex was trimmed
      return;
    }
    
    SCCValue *vertexState = mutableVertexData();
    uint32_t const* phase = getAggregatedValue<uint32_t>(kPhase);
    switch (*phase) {
        
      // let all our connected nodes know we are there
      case SCCPhase::TRANSPOSE: {
        
        vertexState->parents.clear();
        SenderMessage<uint64_t> message(pregelId(), 0);
        sendMessageToAllEdges(message);
        break;
      }
        
      
      // Creates list of parents based on the received ids and halts the vertices
      // that don't have any parent or outgoing edge, hence, they can't be
      // part of an SCC.
      case SCCPhase::TRIMMING:{
        
        for (SenderMessage<uint64_t> const* msg : messages) {
          vertexState->parents.push_back(msg->pregelId);
        }
        // reset the color for vertices which are not active
        vertexState->color = vertexState->vertexID;
        // If this node doesn't have any parents or outgoing edges,
        // it can't be part of an SCC
        RangeIterator<Edge<int32_t>> edges = getEdges();
        if (vertexState->parents.size() == 0 || edges.size() == 0) {
          vertexState->color = INT_MAX;
          voteHalt();
        } else {
          SenderMessage<uint64_t> message(pregelId(), vertexState->color);
          sendMessageToAllEdges(message);
        }
        break;
      }
        
      case SCCPhase::FORWARD_TRAVERSAL:{
        uint64_t old = vertexState->color;
        for (SenderMessage<uint64_t> const* msg : messages) {
          if (vertexState->color < msg->value) {
            vertexState->color = msg->value;
          }
        }
        if (old != vertexState->color) {
          SenderMessage<uint64_t> message(pregelId(), vertexState->color);
          sendMessageToAllEdges(message);
          aggregate(kFoundNewMax, true);
        }
        break;
      }
        
      case SCCPhase::BACKWARD_TRAVERSAL_START:{
        
        // if I am the 'root' of a SCC start traversak
        if (vertexState->vertexID == vertexState->color) {
          SenderMessage<uint64_t> message(pregelId(), vertexState->color);
          // sendMessageToAllParents
          for (PregelID const& pid : vertexState->parents) {
            sendMessage(pid, message);
          }
        }
        break;
      }
        
      case SCCPhase::BACKWARD_TRAVERSAL_REST:{
        
        for (SenderMessage<uint64_t> const* msg : messages) {
          if (vertexState->color == msg->value) {
            for (PregelID const& pid : vertexState->parents) {
              sendMessage(pid, *msg);
            }
            aggregate(kConverged, true);
            voteHalt();
          }
        }
        break;
      }
    }
  }
};

VertexComputation<SCCValue, int32_t,
SenderMessage<uint64_t>>* SCC::createComputation(
    WorkerConfig const* config) const {
  return new SCCComputation();
}


struct SCCGraphFormat : public GraphFormat<SCCValue, int32_t> {
  const std::string _resultField;
  uint64_t vertexIdRange = 0;
  
  SCCGraphFormat(std::string const& result) : _resultField(result) {}
  
  void willLoadVertices(uint64_t count) override {
    // if we aren't running in a cluster it doesn't matter
    if (arangodb::ServerState::instance()->isRunningInCluster()) {
      arangodb::ClusterInfo *ci = arangodb::ClusterInfo::instance();
      if (ci) {
        vertexIdRange = ci->uniqid(count);
      }
    }
  }
  
  size_t estimatedEdgeSize() const override { return 0; };
  
  size_t copyVertexData(std::string const& documentId,
                        arangodb::velocypack::Slice document,
                        SCCValue* targetPtr, size_t maxSize) override {
    SCCValue *senders = (SCCValue*) targetPtr;
    senders->vertexID = vertexIdRange++;
    return sizeof(SCCValue);
  }
  
  size_t copyEdgeData(arangodb::velocypack::Slice document, int32_t* targetPtr,
                      size_t maxSize) override {
    return 0;
  }
  
  bool buildVertexDocument(arangodb::velocypack::Builder& b,
                           const SCCValue* ptr,
                           size_t size) override {
    SCCValue *senders = (SCCValue*) ptr;
    b.add(_resultField, VPackValue(senders->color));
    return true;
  }
  
  bool buildEdgeDocument(arangodb::velocypack::Builder& b, const int32_t* ptr,
                         size_t size) override {
    return false;
  }
};


GraphFormat<SCCValue, int32_t>* SCC::inputFormat() const {
  return new SCCGraphFormat(_resultField);
}

struct SCCMasterContext : public MasterContext {
  SCCMasterContext() {}// TODO use _threashold
  void preGlobalSuperstep() override {
    if (globalSuperstep() == 0) {
      return;
    }
    
    uint32_t const* phase = getAggregatedValue<uint32_t>(kPhase);
    switch (*phase) {
      case SCCPhase::TRANSPOSE:
        LOG(INFO) << "Phase: TRANSPOSE";
        aggregate<uint32_t>(kPhase, SCCPhase::TRIMMING);
        break;
        
      case SCCPhase::TRIMMING:
        LOG(INFO) << "Phase: TRANSPOSE";
        aggregate<uint32_t>(kPhase, SCCPhase::FORWARD_TRAVERSAL);
        break;
        
      case SCCPhase::FORWARD_TRAVERSAL: {
        LOG(INFO) << "Phase: FORWARD_TRAVERSAL";
        bool const* newMaxFound = getAggregatedValue<bool>(kFoundNewMax);
        if (*newMaxFound == false) {
          aggregate<uint32_t>(kPhase, SCCPhase::BACKWARD_TRAVERSAL_START);
        }
      }
        break;
        
      case SCCPhase::BACKWARD_TRAVERSAL_START:
        LOG(INFO) << "Phase: BACKWARD_TRAVERSAL_START";
        aggregate<uint32_t>(kPhase, SCCPhase::BACKWARD_TRAVERSAL_REST);
        break;
        
      case SCCPhase::BACKWARD_TRAVERSAL_REST:
        LOG(INFO) << "Phase: BACKWARD_TRAVERSAL_REST";
        bool const* converged = getAggregatedValue<bool>(kConverged);
        // continue until no more vertices are updated
        if (*converged == false) {
          aggregate<uint32_t>(kPhase, SCCPhase::TRANSPOSE);
        }
        break;
    }
  };
};

MasterContext* SCC::masterContext(VPackSlice userParams) const {
  return new SCCMasterContext();
}

IAggregator* SCC::aggregator(std::string const& name) const {
  if (name == kPhase) {// permanent value
    return new ValueAggregator<uint32_t>(SCCPhase::TRANSPOSE, true);
  } else if (name == kFoundNewMax) {
    return new BoolOrAggregator(false);// non perm
  } else if (name == kConverged) {
    return new BoolOrAggregator(false);// non perm
  }
  return nullptr;
}
