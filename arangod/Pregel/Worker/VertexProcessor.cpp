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

#include "VertexProcessor.h"

#include "Pregel/Algos/ColorPropagation/ColorPropagationValue.h"
#include "Pregel/Algos/DMID/DMIDValue.h"
#include "Pregel/Algos/DMID/DMIDMessage.h"
#include "Pregel/Algos/EffectiveCloseness/ECValue.h"
#include "Pregel/Algos/HITS/HITSValue.h"
#include "Pregel/Algos/HITSKleinberg/HITSKleinbergValue.h"
#include "Pregel/Algos/LabelPropagation/LPValue.h"
#include "Pregel/Algos/SCC/SCCValue.h"
#include "Pregel/Algos/SLPA/SLPAValue.h"
#include "Pregel/Algos/WCC/WCCValue.h"

using namespace arangodb::pregel;

namespace arangodb::pregel {

template<typename V, typename E, typename M>
VertexProcessor<V, E, M>::VertexProcessor(
    std::shared_ptr<WorkerConfig const> workerConfig,
    std::unique_ptr<Algorithm<V, E, M>>& algorithm,
    std::unique_ptr<WorkerContext>& workerContext,
    std::unique_ptr<MessageCombiner<M>>& messageCombiner,
    std::unique_ptr<MessageFormat<M>>& messageFormat, size_t messageBatchSize) {
  if (messageCombiner != nullptr) {
    localMessageCache = std::make_shared<CombiningInCache<M>>(
        containers::FlatHashSet<PregelShard>{}, messageFormat.get(),
        messageCombiner.get());
    outCache = std::make_shared<CombiningOutCache<M>>(
        workerConfig, messageFormat.get(), messageCombiner.get());
  } else {
    localMessageCache = std::make_shared<ArrayInCache<M>>(
        containers::FlatHashSet<PregelShard>{}, messageFormat.get());
    outCache =
        std::make_shared<ArrayOutCache<M>>(workerConfig, messageFormat.get());
  }

  outCache->setBatchSize(messageBatchSize);
  outCache->setLocalCache(localMessageCache.get());

  workerAggregator = std::make_unique<AggregatorHandler>(algorithm.get());

  vertexComputation = std::shared_ptr<VertexComputation<V, E, M>>(
      algorithm->createComputation(workerConfig));
  vertexComputation->_gss = workerConfig->globalSuperstep();
  vertexComputation->_lss = workerConfig->localSuperstep();
  vertexComputation->_context = workerContext.get();
  vertexComputation->_readAggregators = workerContext->_readAggregators.get();

  vertexComputation->_writeAggregators = workerAggregator.get();
  vertexComputation->_cache = outCache.get();
}

template<typename V, typename E, typename M>
VertexProcessor<V, E, M>::~VertexProcessor() {}

template<typename V, typename E, typename M>
auto VertexProcessor<V, E, M>::process(Vertex<V, E>* vertexEntry,
                                       MessageIterator<M> messages) -> void {
  messagesReceived += messages.size();
  memoryBytesUsedForMessages += messages.size() * sizeof(M);

  if (messages.size() > 0 || vertexEntry->active()) {
    vertexComputation->_vertexEntry = vertexEntry;
    vertexComputation->compute(messages);
    if (vertexEntry->active()) {
      activeCount++;
    }
  }
  verticesProcessed += 1;
}

template<typename V, typename E, typename M>
auto VertexProcessor<V, E, M>::result() -> VertexProcessorResult {
  return VertexProcessorResult{
      .activeCount = activeCount,
      .workerAggregator = std::move(workerAggregator),
      .messageStats = MessageStats(outCache->sendCount(), messagesReceived,
                                   memoryBytesUsedForMessages)};
}

}  // namespace arangodb::pregel

// template types to create
template struct arangodb::pregel::VertexProcessor<int64_t, int64_t, int64_t>;
template struct arangodb::pregel::VertexProcessor<uint64_t, uint8_t, uint64_t>;
template struct arangodb::pregel::VertexProcessor<float, float, float>;
template struct arangodb::pregel::VertexProcessor<double, float, double>;
template struct arangodb::pregel::VertexProcessor<float, uint8_t, float>;

// custom algorithm types
template struct arangodb::pregel::VertexProcessor<uint64_t, uint64_t,
                                                  SenderMessage<uint64_t>>;
template struct arangodb::pregel::VertexProcessor<algos::WCCValue, uint64_t,
                                                  SenderMessage<uint64_t>>;
template struct arangodb::pregel::VertexProcessor<algos::SCCValue, int8_t,
                                                  SenderMessage<uint64_t>>;
template struct arangodb::pregel::VertexProcessor<algos::HITSValue, int8_t,
                                                  SenderMessage<double>>;
template struct arangodb::pregel::VertexProcessor<
    algos::HITSKleinbergValue, int8_t, SenderMessage<double>>;
template struct arangodb::pregel::VertexProcessor<algos::ECValue, int8_t,
                                                  HLLCounter>;
template struct arangodb::pregel::VertexProcessor<algos::DMIDValue, float,
                                                  DMIDMessage>;
template struct arangodb::pregel::VertexProcessor<algos::LPValue, int8_t,
                                                  uint64_t>;
template struct arangodb::pregel::VertexProcessor<algos::SLPAValue, int8_t,
                                                  uint64_t>;
template struct arangodb::pregel::VertexProcessor<
    algos::ColorPropagationValue, int8_t, algos::ColorPropagationMessageValue>;
