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
#include "Pregel/Algos/WCC/WCCValue.h"
#include "Pregel/SenderMessage.h"
#include "Pregel/Algos/SCC/SCCValue.h"
#include "Pregel/Algos/HITS/HITSValue.h"
#include "Pregel/Algos/HITSKleinberg/HITSKleinbergValue.h"
#include "Pregel/Algos/EffectiveCloseness/ECValue.h"
#include "Pregel/Algos/DMID/DMIDValue.h"
#include "Pregel/Algos/LabelPropagation/LPValue.h"
#include "Pregel/Algos/SLPA/SLPAValue.h"
#include "Pregel/Algos/ColorPropagation/ColorPropagationValue.h"
#include "Pregel/Algos/DMID/DMIDMessage.h"
#include "Scheduler/SchedulerFeature.h"
#include "Pregel/Worker/VertexProcessor.h"

using namespace arangodb;
using namespace arangodb::pregel;
using namespace arangodb::pregel::worker;

template<typename V, typename E, typename M>
Computing<V, E, M>::Computing(WorkerState<V, E, M>& worker) : worker{worker} {}

template<typename V, typename E, typename M>
auto Computing<V, E, M>::receive(actor::ActorPID const& sender,
                                 actor::ActorPID const& self,
                                 worker::message::WorkerMessages const& message,
                                 Dispatcher dispatcher)
    -> std::unique_ptr<ExecutionState> {
  if (std::holds_alternative<worker::message::PregelMessage>(message)) {
    auto msg = std::get<worker::message::PregelMessage>(message);

    if (msg.gss == worker.config->globalSuperstep()) {
      worker.writeCache->parseMessages(msg);

      return nullptr;
    }

    // if message is for next superstep, resend it (because this worker is
    // still waiting for missing messages in current superstep)
    if (msg.gss == worker.config->globalSuperstep() + 1) {
      dispatcher.dispatchSelf(message);

      return nullptr;
    }

    // otherwise something bad happened
    LOG_TOPIC("da39a", ERR, Logger::PREGEL)
        << "Expected: " << worker.config->globalSuperstep()
        << " Got: " << msg.gss;
    dispatcher.dispatchConductor(
        ResultT<conductor::message::GlobalSuperStepFinished>::error(
            TRI_ERROR_BAD_PARAMETER, "Superstep out of sync"));

    return nullptr;
  }

  if (std::holds_alternative<worker::message::RunGlobalSuperStep>(message)) {
    auto msg = std::get<worker::message::RunGlobalSuperStep>(message);

    dispatcher.dispatchMetrics(
        arangodb::pregel::metrics::message::WorkerGssStarted{.threadsAdded =
                                                                 1});

    // check if worker is in expected gss (previous gss of conductor)
    if (msg.gss != 0 && msg.gss != worker.config->globalSuperstep() + 1) {
      dispatcher.dispatchConductor(
          ResultT<conductor::message::GlobalSuperStepFinished>::error(
              TRI_ERROR_INTERNAL,
              fmt::format("Expected gss {}, but received message with gss {}",
                          worker.config->globalSuperstep() + 1, msg.gss)));

      return nullptr;
    }

    // check if worker received all messages send to it from other workers
    // if not: send RunGlobalsuperstep back to itself such that in between it
    // can receive missing messages
    if (msg.gss != 0 &&
        msg.sendCount != worker.writeCache->containedMessageCount()) {
      LOG_TOPIC("097be", WARN, Logger::PREGEL) << fmt::format(
          "Worker Actor {} in gss {} is waiting for messages: received count "
          "{} != send count {}",
          self, worker.config->_globalSuperstep, msg.sendCount,
          worker.writeCache->containedMessageCount());
      if (not worker.isWaitingForAllMessagesSince.has_value()) {
        worker.isWaitingForAllMessagesSince = std::chrono::steady_clock::now();
      }
      if (std::chrono::steady_clock::now() -
              worker.isWaitingForAllMessagesSince.value() >
          worker.messageTimeout) {
        dispatcher.dispatchConductor(
            ResultT<conductor::message::GlobalSuperStepFinished>::error(
                TRI_ERROR_INTERNAL,
                fmt::format("Worker {} received {} messages in gss {} after "
                            "timeout, although {} were send to it.",
                            self, worker.writeCache->containedMessageCount(),
                            msg.gss, msg.sendCount)));
        return nullptr;
      }
      dispatcher.dispatchSelf(message);

      return nullptr;
    }

    worker.isWaitingForAllMessagesSince = std::nullopt;

    prepareGlobalSuperStep(std::move(msg));
    auto verticesProcessed = processVertices(dispatcher);
    auto out = finishProcessing(verticesProcessed, dispatcher.dispatchStatus);

    dispatcher.dispatchConductor(
        ResultT<conductor::message::GlobalSuperStepFinished>::success(
            std::move(out)));

    dispatcher.dispatchMetrics(
        arangodb::pregel::metrics::message::WorkerGssFinished{
            .threadsRemoved = 1,
            .messagesSent = worker.messageStats.sendCount,
            .messagesReceived = worker.messageStats.receivedCount});

    return nullptr;
  }

  if (std::holds_alternative<worker::message::Store>(message)) {
    dispatcher.dispatchSelf(message);

    return std::make_unique<Storing<V, E, M>>(worker);
  }

  if (std::holds_alternative<worker::message::ProduceResults>(message)) {
    dispatcher.dispatchSelf(message);

    return std::make_unique<ProducingResults<V, E, M>>(worker);
  }

  return std::make_unique<FatalError>();
}

template<typename V, typename E, typename M>
auto Computing<V, E, M>::prepareGlobalSuperStep(
    message::RunGlobalSuperStep message) -> void {
  worker.config->_globalSuperstep = message.gss;
  worker.config->_localSuperstep = message.gss;

  worker.workerContext->_vertexCount = message.vertexCount;
  worker.workerContext->_edgeCount = message.edgeCount;
  if (message.gss == 0) {
    worker.workerContext->preApplication();
  } else {
    TRI_ASSERT(worker.readCache->containedMessageCount() == 0);
    // write cache becomes the readable cache
    std::swap(worker.readCache, worker.writeCache);
  }
  worker.workerContext->_writeAggregators->resetValues();
  worker.workerContext->_readAggregators->setAggregatedValues(
      message.aggregators.slice());
  worker.workerContext->preGlobalSuperstep(message.gss);
}

template<typename V, typename E, typename M>
auto Computing<V, E, M>::processVertices(Dispatcher const& dispatcher)
    -> VerticesProcessed {
  TRI_ASSERT(SchedulerFeature::SCHEDULER != nullptr);
  auto futures = std::vector<futures::Future<ActorVertexProcessorResult>>();
  auto quiverIdx = std::make_shared<std::atomic<size_t>>(0);

  for (auto futureN = size_t{0}; futureN < worker.config->parallelism();
       ++futureN) {
    futures.emplace_back(SchedulerFeature::SCHEDULER->queueWithFuture(
        RequestLane::INTERNAL_LOW, [this, quiverIdx, futureN, dispatcher]() {
          auto processor = ActorVertexProcessor<V, E, M>(
              worker.config, worker.algorithm, worker.workerContext,
              worker.messageCombiner, worker.messageFormat,
              [dispatcher](actor::ActorPID actor,
                           worker::message::PregelMessage message) -> void {
                dispatcher.dispatchOther(actor, message);
              },
              worker.responsibleActorPerShard);

          while (true) {
            auto myCurrentQuiver = quiverIdx->fetch_add(1);
            if (myCurrentQuiver >= worker.magazine.size()) {
              LOG_TOPIC("eef15", DEBUG, Logger::PREGEL) << fmt::format(
                  "No more work left in vertex processor number {}", futureN);
              break;
            }
            for (auto& vertex : *worker.magazine.quivers.at(myCurrentQuiver)) {
              auto messages =
                  worker.readCache->getMessages(vertex.shard(), vertex.key());
              auto status = processor.process(&vertex, messages);

              if (status.verticesProcessed %
                      Utils::batchOfVerticesProcessedBeforeUpdatingStatus ==
                  0) {
                dispatcher.dispatchStatus(
                    pregel::message::GlobalSuperStepUpdate{
                        .gss = worker.config->globalSuperstep(),
                        .verticesProcessed = status.verticesProcessed,
                        .messagesSent = status.messageStats.sendCount,
                        .messagesReceived = status.messageStats.receivedCount,
                        .memoryBytesUsedForMessages =
                            status.messageStats.memoryBytesUsedForMessages});
              }
            }
          }

          processor.outCache->flushMessages();
          worker.writeCache->mergeCache(processor.localMessageCache.get());

          return processor.result();
        }));
  }

  return futures::collectAll(std::move(futures))
      .then([this](auto&& tryResults) {
        auto verticesProcessed = VerticesProcessed{};
        // TODO: exception handling
        auto&& results = tryResults.get();
        for (auto&& tryRes : results) {
          // TODO: exception handling
          auto&& res = tryRes.get();

          worker.workerContext->_writeAggregators->aggregateValues(
              *res.workerAggregator);
          worker.messageStats.accumulate(res.messageStats);
          // TODO why does the VertexProcessor not count receivedCount
          // correctly?
          worker.messageStats.receivedCount =
              worker.readCache->containedMessageCount();

          verticesProcessed.activeCount += res.activeCount;
          for (auto const& [actor, count] : res.sendCountPerActor) {
            verticesProcessed.sendCountPerActor[actor] += count;
          }
        }
        return verticesProcessed;
      })
      .get();
}

template<typename V, typename E, typename M>
auto Computing<V, E, M>::finishProcessing(VerticesProcessed verticesProcessed,
                                          DispatchStatus const& dispatchStatus)
    -> conductor::message::GlobalSuperStepFinished {
  // _feature.metrics()->pregelWorkersRunningNumber->fetch_sub(1);

  worker.workerContext->postGlobalSuperstep(worker.config->_globalSuperstep);

  // all vertices processed
  dispatchStatus(pregel::message::GlobalSuperStepUpdate{
      .gss = worker.config->globalSuperstep(),
      .verticesProcessed = worker.magazine.numberOfVertices(),
      .messagesSent = worker.messageStats.sendCount,
      .messagesReceived = worker.messageStats.receivedCount,
      .memoryBytesUsedForMessages =
          worker.messageStats.memoryBytesUsedForMessages});

  worker.readCache->clear();
  worker.config->_localSuperstep++;

  VPackBuilder aggregators;
  {
    VPackObjectBuilder ob(&aggregators);
    worker.workerContext->_writeAggregators->serializeValues(aggregators);
  }
  std::vector<conductor::message::SendCountPerActor> sendCountList;
  sendCountList.reserve(verticesProcessed.sendCountPerActor.size());
  for (auto const& [actor, count] : verticesProcessed.sendCountPerActor) {
    sendCountList.emplace_back(conductor::message::SendCountPerActor{
        .receiver = actor, .sendCount = count});
  }
  auto gssFinishedEvent = conductor::message::GlobalSuperStepFinished{
      worker.messageStats.sendCount,
      worker.messageStats.receivedCount,
      sendCountList,
      verticesProcessed.activeCount,
      worker.magazine.numberOfVertices(),
      worker.magazine.numberOfEdges(),
      aggregators};
  LOG_TOPIC("ade5b", DEBUG, Logger::PREGEL)
      << fmt::format("Finished GSS: {}", gssFinishedEvent);

  uint64_t tn = worker.config->parallelism();
  uint64_t s = worker.messageStats.sendCount / tn / 2UL;
  worker.messageBatchSize = s > 1000 ? (uint32_t)s : 1000;
  worker.messageStats.reset();
  LOG_TOPIC("a3dbf", TRACE, Logger::PREGEL)
      << fmt::format("Message batch size: {}", worker.messageBatchSize);

  return gssFinishedEvent;
}

// template types to create
template struct arangodb::pregel::worker::Computing<int64_t, int64_t, int64_t>;
template struct arangodb::pregel::worker::Computing<uint64_t, uint8_t,
                                                    uint64_t>;
template struct arangodb::pregel::worker::Computing<float, float, float>;
template struct arangodb::pregel::worker::Computing<double, float, double>;
template struct arangodb::pregel::worker::Computing<float, uint8_t, float>;

// custom algorithm types
template struct arangodb::pregel::worker::Computing<uint64_t, uint64_t,
                                                    SenderMessage<uint64_t>>;
template struct arangodb::pregel::worker::Computing<algos::WCCValue, uint64_t,
                                                    SenderMessage<uint64_t>>;
template struct arangodb::pregel::worker::Computing<algos::SCCValue, int8_t,
                                                    SenderMessage<uint64_t>>;
template struct arangodb::pregel::worker::Computing<algos::HITSValue, int8_t,
                                                    SenderMessage<double>>;
template struct arangodb::pregel::worker::Computing<
    algos::HITSKleinbergValue, int8_t, SenderMessage<double>>;
template struct arangodb::pregel::worker::Computing<algos::ECValue, int8_t,
                                                    HLLCounter>;
template struct arangodb::pregel::worker::Computing<algos::DMIDValue, float,
                                                    DMIDMessage>;
template struct arangodb::pregel::worker::Computing<algos::LPValue, int8_t,
                                                    uint64_t>;
template struct arangodb::pregel::worker::Computing<algos::SLPAValue, int8_t,
                                                    uint64_t>;
template struct arangodb::pregel::worker::Computing<
    algos::ColorPropagationValue, int8_t, algos::ColorPropagationMessageValue>;
