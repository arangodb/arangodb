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
    return std::move(this->state);
  }

  auto operator()(message::LoadGraph msg)
      -> std::unique_ptr<WorkerState<V, E, M>> {
    LOG_TOPIC("cd69c", INFO, Logger::PREGEL)
        << fmt::format("Worker Actor {} is loading", this->self);

    this->state->outCache->setResponsibleActorPerShard(
        msg.responsibleActorPerShard);

    // TODO GORDO-1510
    // _feature.metrics()->pregelWorkersLoadingNumber->fetch_add(1);

    auto graphLoaded = [this]() -> ResultT<conductor::message::GraphLoaded> {
      try {
        auto loader = GraphLoader(
            this->state->config, this->state->algorithm->inputFormat(),
            ActorLoadingUpdate{
                .fn =
                    [this](pregel::message::GraphLoadingUpdate update) -> void {
                  this->template dispatch<pregel::message::StatusMessages>(
                      this->state->statusActor, update);
                }});
        this->state->magazine = loader.load().get();

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
    // TODO GORDO-1510
    // _feature.metrics()->pregelWorkersLoadingNumber->fetch_sub(1);
    return std::move(this->state);
  }

  // ----- computing -----

  auto prepareGlobalSuperStep(message::RunGlobalSuperStep message) -> void {
    // write cache becomes the readable cache
    this->state->outCache->setDispatch(
        [this](actor::ActorPID actor,
               worker::message::PregelMessage message) -> void {
          this->template dispatch<worker::message::WorkerMessages>(actor,
                                                                   message);
        });
    TRI_ASSERT(this->state->readCache->containedMessageCount() == 0);
    std::swap(this->state->readCache, this->state->writeCache);
    this->state->config->_globalSuperstep = message.gss;
    this->state->config->_localSuperstep = message.gss;
    if (message.gss == 0) {
      // now config.gss is set explicitely to 0 and the computation started
      this->state->computationStarted = true;
    }

    this->state->workerContext->_vertexCount = message.vertexCount;
    this->state->workerContext->_edgeCount = message.edgeCount;
    if (message.gss == 0) {
      this->state->workerContext->preApplication();
    }
    this->state->workerContext->_writeAggregators->resetValues();
    this->state->workerContext->_readAggregators->setAggregatedValues(
        message.aggregators.slice());
    this->state->workerContext->preGlobalSuperstep(message.gss);
  }

  void initializeVertexContext(VertexContext<V, E, M>* ctx) {
    ctx->_gss = this->state->config->globalSuperstep();
    ctx->_lss = this->state->config->localSuperstep();
    ctx->_context = this->state->workerContext.get();
    ctx->_readAggregators = this->state->workerContext->_readAggregators.get();
  }

  [[nodiscard]] auto processVertices() -> VerticesProcessed {
    // _feature.metrics()->pregelWorkersRunningNumber->fetch_add(1);

    double start = TRI_microtime();

    InCache<M>* inCache = this->state->inCache.get();
    OutCache<M>* outCache = this->state->outCache.get();
    outCache->setBatchSize(this->state->messageBatchSize);
    outCache->setLocalCache(inCache);

    AggregatorHandler workerAggregator(this->state->algorithm.get());

    std::unique_ptr<VertexComputation<V, E, M>> vertexComputation(
        this->state->algorithm->createComputation(this->state->config));
    initializeVertexContext(vertexComputation.get());
    vertexComputation->_writeAggregators = &workerAggregator;
    vertexComputation->_cache = outCache;

    size_t verticesProcessed = 0;
    size_t activeCount = 0;
    for (auto& quiver : this->state->magazine) {
      for (auto& vertexEntry : *quiver) {
        MessageIterator<M> messages = this->state->readCache->getMessages(
            vertexEntry.shard(), vertexEntry.key());
        this->state->messageStats.receivedCount += messages.size();
        // _feature.metrics()->pregelMessagesReceived->count(
        //     _readCache->containedMessageCount());
        this->state->messageStats.memoryBytesUsedForMessages +=
            messages.size() * sizeof(M);

        if (messages.size() > 0 || vertexEntry.active()) {
          vertexComputation->_vertexEntry = &vertexEntry;
          vertexComputation->compute(messages);
          if (vertexEntry.active()) {
            activeCount++;
          }
        }

        this->state->messageStats.sendCount = outCache->sendCount();
        verticesProcessed++;
        if (verticesProcessed %
                Utils::batchOfVerticesProcessedBeforeUpdatingStatus ==
            0) {
          this->template dispatch<pregel::message::StatusMessages>(
              this->state->statusActor,
              pregel::message::GlobalSuperStepUpdate{
                  .gss = this->state->config->globalSuperstep(),
                  .verticesProcessed = verticesProcessed,
                  .messagesSent = this->state->messageStats.sendCount,
                  .messagesReceived = this->state->messageStats.receivedCount,
                  .memoryBytesUsedForMessages =
                      this->state->messageStats.memoryBytesUsedForMessages});
        }
      }
    }

    outCache->flushMessages();

    this->state->writeCache->mergeCache(inCache);
    // _feature.metrics()->pregelMessagesSent->count(outCache->sendCount());

    this->state->messageStats.superstepRuntimeSecs = TRI_microtime() - start;

    auto out =
        VerticesProcessed{.sendCountPerActor = outCache->sendCountPerActor(),
                          .activeCount = activeCount};

    inCache->clear();
    outCache->clear();

    this->state->workerContext->_writeAggregators->aggregateValues(
        workerAggregator);

    return out;
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
        this->state->messageStats,
        sendCountList,
        verticesProcessed.activeCount,
        this->state->magazine.numberOfVertices(),
        this->state->magazine.numberOfEdges(),
        aggregators};
    LOG_TOPIC("ade5b", DEBUG, Logger::PREGEL)
        << fmt::format("Finished GSS: {}", gssFinishedEvent);

    uint64_t tn = this->state->config->parallelism();
    uint64_t s = this->state->messageStats.sendCount / tn / 2UL;
    this->state->messageBatchSize = s > 1000 ? (uint32_t)s : 1000;
    this->state->messageStats.reset();
    LOG_TOPIC("a3dbf", TRACE, Logger::PREGEL)
        << fmt::format("Message batch size: {}", this->state->messageBatchSize);

    return gssFinishedEvent;
  }

  auto operator()(message::RunGlobalSuperStep message)
      -> std::unique_ptr<WorkerState<V, E, M>> {
    LOG_TOPIC("0f658", INFO, Logger::PREGEL)
        << fmt::format("Worker Actor {} is computing", this->self);

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
    // if not: send message back to itself such that in between it can receive
    // missing messages
    if (message.sendCount != this->state->writeCache->containedMessageCount()) {
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

    // resend all queued messages that were dedicatd to this gss
    for (auto const& message : this->state->messagesForNextGss) {
      this->template dispatch<worker::message::PregelMessage>(this->self,
                                                              message);
    }
    this->state->messagesForNextGss.clear();

    auto verticesProcessed = processVertices();
    auto out = finishProcessing(verticesProcessed);
    this->template dispatch<conductor::message::ConductorMessages>(
        this->state->conductor,
        ResultT<conductor::message::GlobalSuperStepFinished>::success(
            std::move(out)));

    return std::move(this->state);
  }

  auto operator()(message::PregelMessage message)
      -> std::unique_ptr<WorkerState<V, E, M>> {
    LOG_TOPIC("80709", INFO, Logger::PREGEL) << fmt::format(
        "Worker Actor {} with gss {} received message for gss {}", this->self,
        this->state->config->globalSuperstep(), message.gss);
    if (message.gss != this->state->config->globalSuperstep() &&
        message.gss != this->state->config->globalSuperstep() + 1) {
      LOG_TOPIC("da39a", ERR, Logger::PREGEL)
          << "Expected: " << this->state->config->globalSuperstep()
          << " Got: " << message.gss;
      this->template dispatch<conductor::message::ConductorMessages>(
          this->state->conductor,
          ResultT<conductor::message::GlobalSuperStepFinished>::error(
              TRI_ERROR_BAD_PARAMETER, "Superstep out of sync"));
    }
    // queue message if read and write cache are not swapped yet, otherwise
    // you will loose this message
    if (message.gss == this->state->config->globalSuperstep() + 1 ||
        // if config->gss == 0 and _computationStarted == false: read and
        // write cache are not swapped yet, config->gss is currently 0 only
        // due to its initialization
        (this->state->config->globalSuperstep() == 0 &&
         !this->state->computationStarted)) {
      this->state->messagesForNextGss.push_back(message);
    } else {
      this->state->writeCache->parseMessages(message);
    }

    return std::move(this->state);
  }

  // ------ end computing ----

  auto operator()(message::Store msg) -> std::unique_ptr<WorkerState<V, E, M>> {
    LOG_TOPIC("980d9", INFO, Logger::PREGEL)
        << fmt::format("Worker Actor {} is storing", this->self);

    // TODO GORDO-1510
    // _feature.metrics()->pregelWorkersStoringNumber->fetch_add(1);

    auto graphStored = [this]() -> ResultT<conductor::message::Stored> {
      try {
        auto storer = std::make_shared<GraphStorer<V, E>>(
            this->state->config->executionNumber(),
            *this->state->config->vocbase(), this->state->config->parallelism(),
            this->state->algorithm->inputFormat(),
            this->state->config->globalShardIDs(),
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

    // TODO GORDO-1510
    // _feature.metrics()->pregelWorkersStoringNumber->fetch_sub(1);

    this->finish();

    this->template dispatch<conductor::message::ConductorMessages>(
        this->state->conductor, graphStored());

    return std::move(this->state);
  }

  auto operator()(message::ProduceResults msg)
      -> std::unique_ptr<WorkerState<V, E, M>> {
    auto getResults = [this, msg]() -> ResultT<PregelResults> {
      std::function<void()> statusUpdateCallback = [] {
        // TODO GORDO-1584 send update to status actor
        // this->template dispatch<conductor::message::ConductorMessages>(
        //     this->state->conductor,
        //     conductor::message::StatusUpdate{
        //         .executionNumber = this->state->config->executionNumber(),
        //         .status = this->state->observeStatus()});
      };
      try {
        auto storer = std::make_shared<GraphVPackBuilderStorer<V, E>>(
            msg.withID, this->state->config,
            this->state->algorithm->inputFormat(),
            std::move(statusUpdateCallback));
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
