////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <chrono>
#include <memory>
#include "Actor/ActorPID.h"
#include "Actor/HandlerBase.h"
#include "Logger/LogMacros.h"
#include "Pregel/GraphStore/GraphLoader.h"
#include "Pregel/GraphStore/GraphStorer.h"
#include "Pregel/GraphStore/GraphVPackBuilderStorer.h"
#include "Pregel/StatusMessages.h"
#include "Pregel/VertexComputation.h"
#include "Pregel/Worker/Messages.h"
#include "Pregel/Worker/State.h"
#include "Pregel/Conductor/Messages.h"
#include "Pregel/ResultMessages.h"
#include "Pregel/SpawnMessages.h"
#include "Pregel/StatusMessages.h"
#include "Pregel/MetricsMessages.h"
#include "Pregel/Worker/VertexProcessor.h"
#include "Scheduler/SchedulerFeature.h"

namespace arangodb::pregel::worker {

struct VerticesProcessed {
  std::unordered_map<actor::ActorPID, uint64_t> sendCountPerActor;
  size_t activeCount;
};

template<typename V, typename E, typename M, typename Runtime>
struct WorkerHandler : actor::HandlerBase<Runtime, WorkerState<V, E, M>> {
  auto operator()(message::WorkerStart start)
      -> std::unique_ptr<WorkerState<V, E, M>> {
    LOG_TOPIC("cd696", INFO, Logger::PREGEL) << fmt::format(
        "Worker Actor {} started with state {}", this->self, *this->state);
    // TODO GORDO-1556
    // _feature.metrics()->pregelWorkersNumber->fetch_add(1);
    this->template dispatch<conductor::message::ConductorMessages>(
        this->state->conductor, ResultT<conductor::message::WorkerCreated>{});

    this->template dispatch<metrics::message::MetricsMessages>(
        this->state->metricsActor,
        arangodb::pregel::metrics::message::WorkerStarted{});

    return std::move(this->state);
  }

  auto operator()(message::LoadGraph msg)
      -> std::unique_ptr<WorkerState<V, E, M>> {
    LOG_TOPIC("cd69c", INFO, Logger::PREGEL)
        << fmt::format("Worker Actor {} is loading", this->self);

    this->state->responsibleActorPerShard = msg.responsibleActorPerShard;

    this->template dispatch<metrics::message::MetricsMessages>(
        this->state->metricsActor,
        arangodb::pregel::metrics::message::WorkerLoadingStarted{});

    auto graphLoaded = [this]() -> ResultT<conductor::message::GraphLoaded> {
      try {
        auto loader = std::make_shared<GraphLoader<V, E>>(
            this->state->config, this->state->algorithm->inputFormat(),
            ActorLoadingUpdate{
                .fn =
                    [this](pregel::message::GraphLoadingUpdate update) -> void {
                  this->template dispatch<pregel::message::StatusMessages>(
                      this->state->statusActor, update);
                }});
        this->state->magazine = loader->load().get();

        LOG_TOPIC("5206c", WARN, Logger::PREGEL)
            << fmt::format("Worker {} has finished loading.", this->self);
        return {conductor::message::GraphLoaded{
            .executionNumber = this->state->config->executionNumber(),
            .vertexCount = this->state->magazine.numberOfVertices(),
            .edgeCount = this->state->magazine.numberOfEdges()}};
      } catch (std::exception const& ex) {
        return Result{
            TRI_ERROR_INTERNAL,
            fmt::format("caught exception when loading graph: {}", ex.what())};
      } catch (...) {
        return Result{TRI_ERROR_INTERNAL,
                      "caught unknown exception when loading graph"};
      }
    };

    this->template dispatch<conductor::message::ConductorMessages>(
        this->state->conductor, graphLoaded());

    this->template dispatch<metrics::message::MetricsMessages>(
        this->state->metricsActor,
        arangodb::pregel::metrics::message::WorkerLoadingFinished{});
    return std::move(this->state);
  }

  // ----- computing -----

  auto prepareGlobalSuperStep(message::RunGlobalSuperStep message) -> void {
    this->state->config->_globalSuperstep = message.gss;
    this->state->config->_localSuperstep = message.gss;

    this->state->workerContext->_vertexCount = message.vertexCount;
    this->state->workerContext->_edgeCount = message.edgeCount;
    if (message.gss == 0) {
      this->state->workerContext->preApplication();
    } else {
      TRI_ASSERT(this->state->readCache->containedMessageCount() == 0);
      // write cache becomes the readable cache
      std::swap(this->state->readCache, this->state->writeCache);
    }
    this->state->workerContext->_writeAggregators->resetValues();
    this->state->workerContext->_readAggregators->setAggregatedValues(
        message.aggregators.slice());
    this->state->workerContext->preGlobalSuperstep(message.gss);
  }

  [[nodiscard]] auto processVertices() -> VerticesProcessed {
    TRI_ASSERT(SchedulerFeature::SCHEDULER != nullptr);
    auto futures = std::vector<futures::Future<ActorVertexProcessorResult>>();
    auto quiverIdx = std::make_shared<std::atomic<size_t>>(0);

    for (auto futureN = size_t{0}; futureN < this->state->config->parallelism();
         ++futureN) {
      futures.emplace_back(SchedulerFeature::SCHEDULER->queueWithFuture(
          RequestLane::INTERNAL_LOW, [this, quiverIdx, futureN]() {
            auto processor = ActorVertexProcessor<V, E, M>(
                this->state->config, this->state->algorithm,
                this->state->workerContext, this->state->messageCombiner,
                this->state->messageFormat,
                [this](actor::ActorPID actor,
                       worker::message::PregelMessage message) -> void {
                  this->template dispatch<worker::message::WorkerMessages>(
                      actor, message);
                },
                this->state->responsibleActorPerShard);

            while (true) {
              auto myCurrentQuiver = quiverIdx->fetch_add(1);
              if (myCurrentQuiver >= this->state->magazine.size()) {
                LOG_TOPIC("eef15", DEBUG, Logger::PREGEL) << fmt::format(
                    "No more work left in vertex processor number {}", futureN);
                break;
              }
              for (auto& vertex :
                   *this->state->magazine.quivers.at(myCurrentQuiver)) {
                auto messages = this->state->readCache->getMessages(
                    vertex.shard(), vertex.key());
                auto status = processor.process(&vertex, messages);

                if (status.verticesProcessed %
                        Utils::batchOfVerticesProcessedBeforeUpdatingStatus ==
                    0) {
                  this->template dispatch<pregel::message::StatusMessages>(
                      this->state->statusActor,
                      pregel::message::GlobalSuperStepUpdate{
                          .gss = this->state->config->globalSuperstep(),
                          .verticesProcessed = status.verticesProcessed,
                          .messagesSent = status.messageStats.sendCount,
                          .messagesReceived = status.messageStats.receivedCount,
                          .memoryBytesUsedForMessages =
                              status.messageStats.memoryBytesUsedForMessages});
                }
              }
            }

            processor.outCache->flushMessages();
            this->state->writeCache->mergeCache(
                processor.localMessageCache.get());

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

            this->state->workerContext->_writeAggregators->aggregateValues(
                *res.workerAggregator);
            this->state->messageStats.accumulate(res.messageStats);
            // TODO why does the VertexProcessor not count receivedCount
            // correctly?
            this->state->messageStats.receivedCount =
                this->state->readCache->containedMessageCount();

            verticesProcessed.activeCount += res.activeCount;
            for (auto const& [actor, count] : res.sendCountPerActor) {
              verticesProcessed.sendCountPerActor[actor] += count;
            }
          }
          return verticesProcessed;
        })
        .get();
  }

  [[nodiscard]] auto finishProcessing(VerticesProcessed verticesProcessed)
      -> conductor::message::GlobalSuperStepFinished {
    // _feature.metrics()->pregelWorkersRunningNumber->fetch_sub(1);

    this->state->workerContext->postGlobalSuperstep(
        this->state->config->_globalSuperstep);

    // all vertices processed
    this->template dispatch<pregel::message::StatusMessages>(
        this->state->statusActor,
        pregel::message::GlobalSuperStepUpdate{
            .gss = this->state->config->globalSuperstep(),
            .verticesProcessed = this->state->magazine.numberOfVertices(),
            .messagesSent = this->state->messageStats.sendCount,
            .messagesReceived = this->state->messageStats.receivedCount,
            .memoryBytesUsedForMessages =
                this->state->messageStats.memoryBytesUsedForMessages});

    this->state->readCache->clear();
    this->state->config->_localSuperstep++;

    VPackBuilder aggregators;
    {
      VPackObjectBuilder ob(&aggregators);
      this->state->workerContext->_writeAggregators->serializeValues(
          aggregators);
    }
    std::vector<conductor::message::SendCountPerActor> sendCountList;
    for (auto const& [actor, count] : verticesProcessed.sendCountPerActor) {
      sendCountList.emplace_back(conductor::message::SendCountPerActor{
          .receiver = actor, .sendCount = count});
    }
    auto gssFinishedEvent = conductor::message::GlobalSuperStepFinished{
        this->state->messageStats.sendCount,
        this->state->messageStats.receivedCount,
        sendCountList,
        verticesProcessed.activeCount,
        this->state->magazine.numberOfVertices(),
        this->state->magazine.numberOfEdges(),
        aggregators};
    LOG_TOPIC("ade5b", DEBUG, Logger::PREGEL)
        << fmt::format("Finished GSS: {}", gssFinishedEvent);

    uint64_t tn = this->state->config->parallelism();
    uint64_t s = this->state->messageStats.sendCount / tn / 2UL;
    this->state->messageBatchSize = s > 1000 ? (size_t)s : 1000;
    this->state->messageStats.reset();
    LOG_TOPIC("a3dbf", TRACE, Logger::PREGEL)
        << fmt::format("Message batch size: {}", this->state->messageBatchSize);

    return gssFinishedEvent;
  }

  auto operator()(message::RunGlobalSuperStep message)
      -> std::unique_ptr<WorkerState<V, E, M>> {
    LOG_TOPIC("0f658", INFO, Logger::PREGEL) << fmt::format(
        "Worker Actor {} starts computing gss {}", this->self, message.gss);

    this->template dispatch<metrics::message::MetricsMessages>(
        this->state->metricsActor,
        arangodb::pregel::metrics::message::WorkerGssStarted{.threadsAdded =
                                                                 1});

    // check if worker is in expected gss (previous gss of conductor)
    if (message.gss != 0 &&
        message.gss != this->state->config->globalSuperstep() + 1) {
      this->template dispatch<conductor::message::ConductorMessages>(
          this->state->conductor,
          ResultT<conductor::message::GlobalSuperStepFinished>::error(
              TRI_ERROR_INTERNAL,
              fmt::format("Expected gss {}, but received message with gss {}",
                          this->state->config->globalSuperstep() + 1,
                          message.gss)));
      return std::move(this->state);
    }

    // check if worker received all messages send to it from other workers
    // if not: send RunGlobalsuperstep back to itself such that in between it
    // can receive missing messages
    if (message.gss != 0 &&
        message.sendCount != this->state->writeCache->containedMessageCount()) {
      LOG_TOPIC("097be", WARN, Logger::PREGEL) << fmt::format(
          "Worker Actor {} in gss {} is waiting for messages: received count "
          "{} != send count {}",
          this->self, this->state->config->_globalSuperstep, message.sendCount,
          this->state->writeCache->containedMessageCount());
      if (not this->state->isWaitingForAllMessagesSince.has_value()) {
        this->state->isWaitingForAllMessagesSince =
            std::chrono::steady_clock::now();
      }
      if (std::chrono::steady_clock::now() -
              this->state->isWaitingForAllMessagesSince.value() >
          this->state->messageTimeout) {
        this->template dispatch<conductor::message::ConductorMessages>(
            this->state->conductor,
            ResultT<conductor::message::GlobalSuperStepFinished>::error(
                TRI_ERROR_INTERNAL,
                fmt::format("Worker {} received {} messages in gss {} after "
                            "timeout, although {} were send to it.",
                            this->self,
                            this->state->writeCache->containedMessageCount(),
                            message.gss, message.sendCount)));
        return std::move(this->state);
      }
      this->template dispatch<worker::message::WorkerMessages>(this->self,
                                                               message);
      return std::move(this->state);
    }
    this->state->isWaitingForAllMessagesSince = std::nullopt;

    prepareGlobalSuperStep(std::move(message));
    auto verticesProcessed = processVertices();
    auto out = finishProcessing(verticesProcessed);
    this->template dispatch<conductor::message::ConductorMessages>(
        this->state->conductor,
        ResultT<conductor::message::GlobalSuperStepFinished>::success(
            std::move(out)));

    this->template dispatch<metrics::message::MetricsMessages>(
        this->state->metricsActor,
        arangodb::pregel::metrics::message::WorkerGssFinished{
            .threadsRemoved = 1,
            .messagesSent = this->state->messageStats.sendCount,
            .messagesReceived = this->state->messageStats.receivedCount});

    return std::move(this->state);
  }

  auto operator()(message::PregelMessage message)
      -> std::unique_ptr<WorkerState<V, E, M>> {
    if (message.gss == this->state->config->globalSuperstep()) {
      this->state->writeCache->parseMessages(message);
      return std::move(this->state);
    }

    // if message is for next superstep, resend it (because this worker is
    // still waiting for missing messages in current superstep)
    if (message.gss == this->state->config->globalSuperstep() + 1) {
      this->template dispatch<worker::message::WorkerMessages>(this->self,
                                                               message);
      return std::move(this->state);
    }

    // otherwise something bad happened
    LOG_TOPIC("da39a", ERR, Logger::PREGEL)
        << "Expected: " << this->state->config->globalSuperstep()
        << " Got: " << message.gss;
    this->template dispatch<conductor::message::ConductorMessages>(
        this->state->conductor,
        ResultT<conductor::message::GlobalSuperStepFinished>::error(
            TRI_ERROR_BAD_PARAMETER, "Superstep out of sync"));
    return std::move(this->state);
  }

  // ------ end computing ----

  auto operator()(message::Store msg) -> std::unique_ptr<WorkerState<V, E, M>> {
    LOG_TOPIC("980d9", INFO, Logger::PREGEL)
        << fmt::format("Worker Actor {} is storing", this->self);

    this->template dispatch<metrics::message::MetricsMessages>(
        this->state->metricsActor,
        arangodb::pregel::metrics::message::WorkerStoringStarted{});

    auto graphStored = [this]() -> ResultT<conductor::message::Stored> {
      try {
        auto storer = std::make_shared<GraphStorer<V, E>>(
            this->state->config->executionNumber(),
            *this->state->config->vocbase(), this->state->config->parallelism(),
            this->state->algorithm->inputFormat(),
            this->state->config->graphSerdeConfig(),
            ActorStoringUpdate{
                .fn =
                    [this](pregel::message::GraphStoringUpdate update) -> void {
                  this->template dispatch<pregel::message::StatusMessages>(
                      this->state->statusActor, update);
                }});
        storer->store(this->state->magazine).get();
        return conductor::message::Stored{};
      } catch (std::exception const& ex) {
        return Result{
            TRI_ERROR_INTERNAL,
            fmt::format("caught exception when storing graph: {}", ex.what())};
      } catch (...) {
        return Result{TRI_ERROR_INTERNAL,
                      "caught unknown exception when storing graph"};
      }
    };

    this->template dispatch<metrics::message::MetricsMessages>(
        this->state->metricsActor,
        arangodb::pregel::metrics::message::WorkerStoringFinished{});

    this->template dispatch<conductor::message::ConductorMessages>(
        this->state->conductor, graphStored());

    return std::move(this->state);
  }

  auto operator()(message::ProduceResults msg)
      -> std::unique_ptr<WorkerState<V, E, M>> {
    auto getResults = [this, msg]() -> ResultT<PregelResults> {
      try {
        auto storer = std::make_shared<GraphVPackBuilderStorer<V, E>>(
            msg.withID, this->state->config,
            this->state->algorithm->inputFormat());
        storer->store(this->state->magazine).get();
        return PregelResults{*storer->stealResult()};
      } catch (std::exception const& ex) {
        return Result{TRI_ERROR_INTERNAL,
                      fmt::format("caught exception when receiving results: {}",
                                  ex.what())};
      } catch (...) {
        return Result{TRI_ERROR_INTERNAL,
                      "caught unknown exception when receiving results"};
      }
    };
    auto results = getResults();
    this->template dispatch<pregel::message::ResultMessages>(
        this->state->resultActor,
        pregel::message::SaveResults{.results = {results}});
    this->template dispatch<pregel::conductor::message::ConductorMessages>(
        this->state->conductor,
        pregel::conductor::message::ResultCreated{.results = {results}});

    return std::move(this->state);
  }

  auto operator()(message::Cleanup msg)
      -> std::unique_ptr<WorkerState<V, E, M>> {
    LOG_TOPIC("664f5", INFO, Logger::PREGEL)
        << fmt::format("Worker Actor {} is cleaned", this->self);

    this->finish();

    this->template dispatch<pregel::message::SpawnMessages>(
        this->state->spawnActor, pregel::message::SpawnCleanup{});
    this->template dispatch<pregel::conductor::message::ConductorMessages>(
        this->state->conductor, pregel::conductor::message::CleanupFinished{});
    this->template dispatch<metrics::message::MetricsMessages>(
        this->state->metricsActor,
        arangodb::pregel::metrics::message::WorkerFinished{});

    return std::move(this->state);
  }

  auto operator()(actor::message::UnknownMessage unknown)
      -> std::unique_ptr<WorkerState<V, E, M>> {
    LOG_TOPIC("7ee4d", INFO, Logger::PREGEL) << fmt::format(
        "Worker Actor: Error - sent unknown message to {}", unknown.receiver);
    return std::move(this->state);
  }

  auto operator()(actor::message::ActorNotFound notFound)
      -> std::unique_ptr<WorkerState<V, E, M>> {
    LOG_TOPIC("2d647", INFO, Logger::PREGEL) << fmt::format(
        "Worker Actor: Error - receiving actor {} not found", notFound.actor);
    return std::move(this->state);
  }

  auto operator()(actor::message::NetworkError notFound)
      -> std::unique_ptr<WorkerState<V, E, M>> {
    LOG_TOPIC("1c3d9", INFO, Logger::PREGEL) << fmt::format(
        "Worker Actor: Error - network error {}", notFound.message);
    return std::move(this->state);
  }

  auto operator()(auto&& rest) -> std::unique_ptr<WorkerState<V, E, M>> {
    LOG_TOPIC("8b81a", INFO, Logger::PREGEL)
        << fmt::format("Worker Actor: Got unhandled message: {}", rest);
    return std::move(this->state);
  }
};

}  // namespace arangodb::pregel::worker
