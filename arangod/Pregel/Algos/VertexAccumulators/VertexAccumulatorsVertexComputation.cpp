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
/// @author Heiko Kernbach
/// @author Lars Maier
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include "VertexAccumulators.h"

using namespace arangodb::pregel::algos;

VertexAccumulators::VertexComputation::VertexComputation() {}

void VertexAccumulators::VertexComputation::compute(MessageIterator<MessageData> const& messages) {
  // auto currentComponent = vertexData();

  if (globalSuperstep() > 0) {
    bool halt = true;

    /*
      for (const MessageData* msg : messages) {
      if (msg->value < currentComponent) {
      currentComponent = msg->value;
      // TODO: optimization update the edge value if present
      // problem: there might be loads of edges, could be expensive
      }
      }

      auto message = (pregelId(), currentComponent);
      for (const MessageData* msg : messages) {
      if (msg->value > currentComponent) {
      TRI_ASSERT(msg->senderId != pregelId());
          sendMessage(msg->senderId, message);
          halt = false;
        }
      }
      */
    if (halt) {
      voteHalt();
    } else {
      voteActive();
    }
  }

  /*
  MessageData message(pregelId(), currentComponent);
  RangeIterator<Edge<uint64_t>> edges = this->getEdges();
  for (; edges.hasMore(); ++edges) {
    Edge<uint64_t>* edge = *edges;
    if (edge->toKey() == this->key()) {
      continue;  // no need to send message to self
    }

    // remember the value we send
    edge->data() = currentComponent;

    sendMessage(edge, message);
  }
  */
}
