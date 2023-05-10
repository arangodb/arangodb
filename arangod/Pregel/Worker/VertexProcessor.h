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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <Pregel/Algorithm.h>
#include <Pregel/Iterators.h>
#include <Pregel/MessageCombiner.h>
#include <Pregel/GraphStore/Quiver.h>
#include <Pregel/Worker/WorkerConfig.h>
#include <Pregel/IncomingCache.h>
#include <Pregel/OutgoingCache.h>
#include <Pregel/VertexComputation.h>
#include <Pregel/WorkerContext.h>

namespace arangodb::pregel {

struct VertexProcessorResult {
  uint64_t activeCount;
  std::unique_ptr<AggregatorHandler> workerAggregator;
  MessageStats messageStats;
};
//
// A vertex processor is a means to combine all infrastructure needed to
// process batches of vertices; this is employed in Worker.cpp
// to create multiple fibers to process vertices.
//
// A vertex processor is designed and supposed to run as a single thread of
// computation
//
template<typename V, typename E, typename M>
struct VertexProcessor {
  VertexProcessor(std::shared_ptr<WorkerConfig const> workerConfig,
                  std::unique_ptr<Algorithm<V, E, M>>& algorithm,
                  std::unique_ptr<WorkerContext>& workerContext,
                  std::unique_ptr<MessageCombiner<M>>& messageCombiner,
                  std::unique_ptr<MessageFormat<M>>& messageFormat);
  ~VertexProcessor();

  auto process(Vertex<V, E>* vertexEntry, MessageIterator<M> messages) -> void;
  [[nodiscard]] auto result() -> VertexProcessorResult;

  // aggregators
  size_t activeCount = 0;
  size_t messagesReceived = 0;
  size_t memoryBytesUsedForMessages = 0;
  size_t verticesProcessed = 0;

  std::shared_ptr<OutCache<M>> outCache;
  // The outCache handles dispatching messages and will
  // queue messages that go to shards deemed local into
  // the localMessageCache
  std::shared_ptr<InCache<M>> localMessageCache;
  std::shared_ptr<VertexComputation<V, E, M>> vertexComputation;
  std::unique_ptr<AggregatorHandler> workerAggregator;

  uint32_t messageBatchSize = 5000;
};

}  // namespace arangodb::pregel
