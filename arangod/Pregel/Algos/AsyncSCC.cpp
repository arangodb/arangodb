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

#include "AsyncSCC.h"

#include "Basics/system-functions.h"
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

struct ASCCComputation final
    : public VertexComputation<SCCValue, int8_t, SenderMessage<uint64_t>> {
  ASCCComputation() {}

  void compute(MessageIterator<SenderMessage<uint64_t>> const& messages) override {
    if (isActive() == false) {
      // color was already determinded or vertex was trimmed
      return;
    }

    SCCValue* vertexState = mutableVertexData();
    uint32_t const* phase = getAggregatedValue<uint32_t>(kPhase);
    switch (*phase) {
      // let all our connected nodes know we are there
      case SCCPhase::TRANSPOSE: {
        // only one step in this phase
        enterNextGlobalSuperstep();

        vertexState->parents.clear();
        SenderMessage<uint64_t> message(pregelId(), 0);
        sendMessageToAllNeighbours(message);
        break;
      }

      // Creates list of parents based on the received ids and halts the
      // vertices
      // that don't have any parent or outgoing edge, hence, they can't be
      // part of an SCC.
      case SCCPhase::TRIMMING: {
        // only one step in this phase
        enterNextGlobalSuperstep();

        for (SenderMessage<uint64_t> const* msg : messages) {
          vertexState->parents.push_back(msg->senderId);
        }
        // reset the color for vertices which are not active
        vertexState->color = vertexState->vertexID;
        // If this node doesn't have any parents or outgoing edges,
        // it can't be part of an SCC
        if (vertexState->parents.size() == 0 || getEdgeCount() == 0) {
          voteHalt();
        } else {
          SenderMessage<uint64_t> message(pregelId(), vertexState->color);
          sendMessageToAllNeighbours(message);
        }
        break;
      }

      // converging phase
      case SCCPhase::FORWARD_TRAVERSAL: {
        uint64_t old = vertexState->color;
        for (SenderMessage<uint64_t> const* msg : messages) {
          if (vertexState->color < msg->value) {
            vertexState->color = msg->value;
          }
        }
        if (old != vertexState->color) {
          SenderMessage<uint64_t> message(pregelId(), vertexState->color);
          sendMessageToAllNeighbours(message);
          aggregate(kFoundNewMax, true);
        }
        break;
      }

      case SCCPhase::BACKWARD_TRAVERSAL_START: {
        // only one step in this phase
        enterNextGlobalSuperstep();

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

      // converging phase
      case SCCPhase::BACKWARD_TRAVERSAL_REST: {
        for (SenderMessage<uint64_t> const* msg : messages) {
          if (vertexState->color == msg->value) {
            for (PregelID const& pid : vertexState->parents) {
              sendMessage(pid, *msg);
            }
            aggregate(kConverged, true);
            voteHalt();
            break;
          }
        }
        break;
      }
    }
  }
};

VertexComputation<SCCValue, int8_t, SenderMessage<uint64_t>>* AsyncSCC::createComputation(
    WorkerConfig const* config) const {
  return new ASCCComputation();
}

namespace {

struct SCCGraphFormat : public GraphFormat<SCCValue, int8_t> {
  const std::string _resultField;

  explicit SCCGraphFormat(application_features::ApplicationServer& server,
                          std::string const& result)
      : GraphFormat<SCCValue, int8_t>(server), _resultField(result) {}

  size_t estimatedEdgeSize() const override { return 0; }

  void copyVertexData(std::string const& /*documentId*/, arangodb::velocypack::Slice /*document*/,
                      SCCValue& targetPtr, uint64_t& vertexIdRange) override {
    targetPtr.vertexID = vertexIdRange++;
  }

  bool buildVertexDocument(arangodb::velocypack::Builder& b, SCCValue const* ptr) const override {
    b.add(_resultField, VPackValue(ptr->color));
    return true;
  }
};

}  // namespace

GraphFormat<SCCValue, int8_t>* AsyncSCC::inputFormat() const {
  return new SCCGraphFormat(_server, _resultField);
}

struct ASCCMasterContext : public MasterContext {
  ASCCMasterContext() {}  // TODO use _threashold
  void preGlobalSuperstep() override {
    if (globalSuperstep() == 0) {
      enterNextGlobalSuperstep();
      return;
    }

    uint32_t const* phase = getAggregatedValue<uint32_t>(kPhase);
    switch (*phase) {
      case SCCPhase::TRANSPOSE:
        LOG_TOPIC("b0431", DEBUG, Logger::PREGEL) << "Phase: TRIMMING";
        enterNextGlobalSuperstep();
        aggregate<uint32_t>(kPhase, SCCPhase::TRIMMING);
        break;

      case SCCPhase::TRIMMING:
        LOG_TOPIC("44a2f", DEBUG, Logger::PREGEL) << "Phase: FORWARD_TRAVERSAL";
        enterNextGlobalSuperstep();
        aggregate<uint32_t>(kPhase, SCCPhase::FORWARD_TRAVERSAL);
        break;

      case SCCPhase::FORWARD_TRAVERSAL: {
        bool const* newMaxFound = getAggregatedValue<bool>(kFoundNewMax);
        if (*newMaxFound == false) {
          LOG_TOPIC("14832", DEBUG, Logger::PREGEL) << "Phase: BACKWARD_TRAVERSAL_START";
          aggregate<uint32_t>(kPhase, SCCPhase::BACKWARD_TRAVERSAL_START);
        }
      } break;

      case SCCPhase::BACKWARD_TRAVERSAL_START:
        LOG_TOPIC("8d480", DEBUG, Logger::PREGEL) << "Phase: BACKWARD_TRAVERSAL_REST";
        aggregate<uint32_t>(kPhase, SCCPhase::BACKWARD_TRAVERSAL_REST);
        break;

      case SCCPhase::BACKWARD_TRAVERSAL_REST:
        bool const* converged = getAggregatedValue<bool>(kConverged);
        // continue until no more vertices are updated
        if (*converged == false) {
          LOG_TOPIC("a9542", DEBUG, Logger::PREGEL) << "Phase: TRANSPOSE";
          aggregate<uint32_t>(kPhase, SCCPhase::TRANSPOSE);
        }
        break;
    }
  };

  void postLocalSuperstep() override {
    uint32_t const* phase = getAggregatedValue<uint32_t>(kPhase);
    if (*phase == SCCPhase::FORWARD_TRAVERSAL) {
      bool const* newMaxFound = getAggregatedValue<bool>(kFoundNewMax);
      if (*newMaxFound == false) {
        enterNextGlobalSuperstep();
      }
    } else if (*phase == SCCPhase::BACKWARD_TRAVERSAL_REST) {
      bool const* converged = getAggregatedValue<bool>(kConverged);
      // continue until no more vertices are updated
      if (*converged == false) {
        enterNextGlobalSuperstep();
      }
    }
  };
};

MasterContext* AsyncSCC::masterContext(VPackSlice userParams) const {
  return new ASCCMasterContext();
}

IAggregator* AsyncSCC::aggregator(std::string const& name) const {
  if (name == kPhase) {  // permanent value
    return new OverwriteAggregator<uint32_t>(SCCPhase::TRANSPOSE, true);
  } else if (name == kFoundNewMax) {
    return new BoolOrAggregator(false);  // non perm
  } else if (name == kConverged) {
    return new BoolOrAggregator(false);  // non perm
  }
  return nullptr;
}
