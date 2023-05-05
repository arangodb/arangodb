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
/// @author Aditya Mukhopadhyay
////////////////////////////////////////////////////////////////////////////////

#include "ComputingState.h"

#include <utility>
#include "FatalErrorState.h"
#include "Pregel/Worker/State.h"
#include "StoringState.h"
#include "ProducingResultsState.h"

using namespace arangodb;
using namespace arangodb::pregel;
using namespace arangodb::pregel::worker;

template<typename V, typename E, typename M>
Computing<V, E, M>::Computing(actor::ActorPID self,
                              WorkerState<V, E, M>& worker)
    : self(std::move(self)), worker{worker} {}

template<typename V, typename E, typename M>
auto Computing<V, E, M>::receive(actor::ActorPID const& sender,
                                 worker::message::WorkerMessages const& message,
                                 Dispatcher dispatcher)
    -> std::unique_ptr<ExecutionState> {
  if (std::holds_alternative<worker::message::PregelMessage>(message)) {
    auto msg = std::get<worker::message::PregelMessage>(message);

    if (msg.gss != worker->config->globalSuperstep() &&
        msg.gss != worker->config->globalSuperstep() + 1) {
      LOG_TOPIC("da39a", ERR, Logger::PREGEL)
          << "Expected: " << worker->config->globalSuperstep()
          << " Got: " << msg.gss;
      dispatcher.dispatchConductor(
          ResultT<conductor::message::GlobalSuperStepFinished>::error(
              TRI_ERROR_BAD_PARAMETER, "Superstep out of sync"));
    }
    // queue message if read and write cache are not swapped yet, otherwise
    // you will lose this message
    if (msg.gss == worker->config->globalSuperstep() + 1 ||
        // if config->gss == 0 and _computationStarted == false: read and
        // write cache are not swapped yet, config->gss is currently 0 only
        // due to its initialization
        (worker->config->globalSuperstep() == 0 &&
         !worker->computationStarted)) {
      worker->messagesForNextGss.push_back(message);
    } else {
      worker->writeCache->parseMessages(message);
    }

    return nullptr;
  }

  if (std::holds_alternative<worker::message::RunGlobalSuperStep>(message)) {
    auto msg = std::get<worker::message::RunGlobalSuperStep>(message);

    dispatcher.dispatchMetrics(
        arangodb::pregel::metrics::message::WorkerGssStarted{.threadsAdded =
                                                                 1});

    // check if worker is in expected gss (previous gss of conductor)
    if (msg.gss != 0 && msg.gss != worker->config->globalSuperstep() + 1) {
      dispatchConductor(
          ResultT<conductor::message::GlobalSuperStepFinished>::error(
              TRI_ERROR_INTERNAL,
              fmt::format("Expected gss {}, but received message with gss {}",
                          worker->config->globalSuperstep() + 1, msg.gss)));

      return nullptr;
    }

    // check if worker received all messages send to it from other workers
    // if not: send message back to itself such that in between it can receive
    // missing messages
    if (msg.sendCount != worker->writeCache->containedMessageCount()) {
      if (not worker->isWaitingForAllMessagesSince.has_value()) {
        worker->isWaitingForAllMessagesSince = std::chrono::steady_clock::now();
      }
      if (std::chrono::steady_clock::now() -
              worker->isWaitingForAllMessagesSince.value() >
          worker->messageTimeout) {
        dispatchConductor(
            ResultT<conductor::message::GlobalSuperStepFinished>::error(
                TRI_ERROR_INTERNAL,
                fmt::format("Worker {} received {} messages in gss {} after "
                            "timeout, although {} were send to it.",
                            self, worker->writeCache->containedMessageCount(),
                            msg.gss, msg.sendCount)));

        return nullptr;
      }
      dispatcher.dispatchSelf(message);

      return nullptr;
    }

    worker->isWaitingForAllMessagesSince = std::nullopt;

    prepareGlobalSuperStep(std::move(msg));

    // resend all queued messages that were dedicated to this gss
    for (auto const& nextGssMsg : this->state->messagesForNextGss) {
      dispatchSelf(nextGssMsg);
    }
    worker->messagesForNextGss.clear();

    auto verticesProcessed = processVertices(dispatcher.dispatchStatus);
    auto out = finishProcessing(verticesProcessed, dispatcher.dispatchStatus);
    dispatchConductor(
        ResultT<conductor::message::GlobalSuperStepFinished>::success(
            std::move(out)));

    dispatcher.dispatchMetrics(
        arangodb::pregel::metrics::message::WorkerGssFinished{
            .threadsRemoved = 1,
            .messagesSent = worker->messageStats.sendCount,
            .messagesReceived = worker->messageStats.receivedCount});

    return nullptr;
  }

  if (std::holds_alternative<worker::message::Store>(message)) {
    dispatcher.dispatchSelf(message);

    return std::make_unique<Storing>(self, worker);
  }

  if (std::holds_alternative<worker::message::ProduceResults>(message)) {
    dispatcher.dispatchSelf(message);

    return std::make_unique<ProducingResults>(self, worker);
  }

  return std::make_unique<FatalError>();
}

template<typename V, typename E, typename M>
auto Computing<V, E, M>::prepareGlobalSuperStep(
    message::RunGlobalSuperStep message, DispatchOther const& dispatchOther)
    -> void {
  // write cache becomes the readable cache
  worker->outCache->setDispatch(dispatchOther);
  TRI_ASSERT(worker->readCache->containedMessageCount() == 0);
  std::swap(worker->readCache, worker->writeCache);
  worker->config->_globalSuperstep = message.gss;
  worker->config->_localSuperstep = message.gss;
  if (message.gss == 0) {
    // now config.gss is set explicitely to 0 and the computation started
    worker->computationStarted = true;
  }

  worker->workerContext->_vertexCount = message.vertexCount;
  worker->workerContext->_edgeCount = message.edgeCount;
  if (message.gss == 0) {
    worker->workerContext->preApplication();
  }
  worker->workerContext->_writeAggregators->resetValues();
  worker->workerContext->_readAggregators->setAggregatedValues(
      message.aggregators.slice());
  worker->workerContext->preGlobalSuperstep(message.gss);
}

template<typename V, typename E, typename M>
auto Computing<V, E, M>::processVertices(DispatchStatus const& dispatchStatus)
    -> VerticesProcessed {
  double start = TRI_microtime();

  InCache<M>* inCache = worker->inCache.get();
  OutCache<M>* outCache = worker->outCache.get();
  outCache->setBatchSize(worker->messageBatchSize);
  outCache->setLocalCache(inCache);

  AggregatorHandler workerAggregator(worker->algorithm.get());

  std::unique_ptr<VertexComputation<V, E, M>> vertexComputation(
      worker->algorithm->createComputation(worker->config));
  initializeVertexContext(vertexComputation.get());
  vertexComputation->_writeAggregators = &workerAggregator;
  vertexComputation->_cache = outCache;

  size_t verticesProcessed = 0;
  size_t activeCount = 0;
  for (auto& quiver : worker->magazine) {
    for (auto& vertexEntry : *quiver) {
      MessageIterator<M> messages = worker->readCache->getMessages(
          vertexEntry.shard(), vertexEntry.key());
      worker->messageStats.receivedCount += messages.size();
      worker->messageStats.memoryBytesUsedForMessages +=
          messages.size() * sizeof(M);

      if (messages.size() > 0 || vertexEntry.active()) {
        vertexComputation->_vertexEntry = &vertexEntry;
        vertexComputation->compute(messages);
        if (vertexEntry.active()) {
          activeCount++;
        }
      }

      worker->messageStats.sendCount = outCache->sendCount();
      verticesProcessed++;
      if (verticesProcessed %
              Utils::batchOfVerticesProcessedBeforeUpdatingStatus ==
          0) {
        dispatchStatus(pregel::message::GlobalSuperStepUpdate{
            .gss = worker->config->globalSuperstep(),
            .verticesProcessed = verticesProcessed,
            .messagesSent = worker->messageStats.sendCount,
            .messagesReceived = worker->messageStats.receivedCount,
            .memoryBytesUsedForMessages =
                worker->messageStats.memoryBytesUsedForMessages});
      }
    }
  }

  outCache->flushMessages();

  worker->writeCache->mergeCache(inCache);
  worker->messageStats.superstepRuntimeSecs = TRI_microtime() - start;

  auto out =
      VerticesProcessed{.sendCountPerActor = outCache->sendCountPerActor(),
                        .activeCount = activeCount};

  inCache->clear();
  outCache->clear();

  worker->workerContext->_writeAggregators->aggregateValues(workerAggregator);

  return out;
}

template<typename V, typename E, typename M>
auto Computing<V, E, M>::finishProcessing(VerticesProcessed verticesProcessed,
                                          DispatchStatus const& dispatchStatus)
    -> conductor::message::GlobalSuperStepFinished {
  worker->workerContext->postGlobalSuperstep(worker->config->_globalSuperstep);

  // all vertices processed
  dispatchStatus(pregel::message::GlobalSuperStepUpdate{
      .gss = worker->config->globalSuperstep(),
      .verticesProcessed = worker->magazine.numberOfVertices(),
      .messagesSent = worker->messageStats.sendCount,
      .messagesReceived = worker->messageStats.receivedCount,
      .memoryBytesUsedForMessages =
          worker->messageStats.memoryBytesUsedForMessages});

  worker->readCache->clear();
  worker->config->_localSuperstep++;

  VPackBuilder aggregators;
  {
    VPackObjectBuilder ob(&aggregators);
    worker->workerContext->_writeAggregators->serializeValues(aggregators);
  }
  std::vector<conductor::message::SendCountPerActor> sendCountList;
  sendCountList.reserve(verticesProcessed.sendCountPerActor.size());
  for (auto const& [actor, count] : verticesProcessed.sendCountPerActor) {
    sendCountList.emplace_back(conductor::message::SendCountPerActor{
        .receiver = actor, .sendCount = count});
  }
  auto gssFinishedEvent = conductor::message::GlobalSuperStepFinished{
      worker->messageStats,
      sendCountList,
      verticesProcessed.activeCount,
      worker->magazine.numberOfVertices(),
      worker->magazine.numberOfEdges(),
      aggregators};
  LOG_TOPIC("ade5b", DEBUG, Logger::PREGEL)
      << fmt::format("Finished GSS: {}", gssFinishedEvent);

  uint64_t tn = worker->config->parallelism();
  uint64_t s = worker->messageStats.sendCount / tn / 2UL;
  worker->messageBatchSize = s > 1000 ? (uint32_t)s : 1000;
  worker->messageStats.reset();
  LOG_TOPIC("a3dbf", TRACE, Logger::PREGEL)
      << fmt::format("Message batch size: {}", worker->messageBatchSize);

  return gssFinishedEvent;
}

template<typename V, typename E, typename M>
void Computing<V, E, M>::initializeVertexContext(VertexContext<V, E, M>* ctx) {
  ctx->_gss = worker->config->globalSuperstep();
  ctx->_lss = worker->config->localSuperstep();
  ctx->_context = worker->workerContext.get();
  ctx->_readAggregators = worker->workerContext->_readAggregators.get();
}
