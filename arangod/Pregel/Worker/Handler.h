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
#include "Pregel/Worker/ExecutionStates/State.h"
#include "Pregel/Worker/ExecutionStates/CleanedUpState.h"

namespace arangodb::pregel::worker {

struct VerticesProcessed {
  std::unordered_map<actor::ActorPID, uint64_t> sendCountPerActor;
  size_t activeCount;
};

template<typename V, typename E, typename M, typename Runtime>
struct WorkerHandler : actor::HandlerBase<Runtime, WorkerState<V, E, M>> {
  DispatchStatus const& dispatchStatus =
      [this](pregel::message::StatusMessages message) -> void {
    this->template dispatch<pregel::message::StatusMessages>(
        this->state->statusActor, message);
  };
  DispatchMetrics const& dispatchMetrics =
      [this](metrics::message::MetricsMessages message) -> void {
    this->template dispatch<metrics::message::MetricsMessages>(
        this->state->metricsActor, message);
  };
  DispatchConductor const& dispatchConductor =
      [this](conductor::message::ConductorMessages message) -> void {
    this->template dispatch<conductor::message::ConductorMessages>(
        this->state->conductor, message);
  };
  DispatchSelf const& dispatchSelf =
      [this](message::WorkerMessages message) -> void {
    this->template dispatch<message::WorkerMessages>(this->self, message);
  };
  DispatchOther const& dispatchOther =
      this->template dispatch<message::WorkerMessages>;

  auto operator()(message::WorkerStart start)
      -> std::unique_ptr<WorkerState<V, E, M>> {
    LOG_TOPIC("cd696", INFO, Logger::PREGEL) << fmt::format(
        "Worker Actor {} started with state {}", this->self, *this->state);

    auto newState = this->executionState->receive(
        this->sender, start, dispatchStatus, dispatchMetrics, dispatchConductor,
        dispatchSelf);
    changeState(newState);

    return std::move(this->state);
  }

  auto operator()(message::LoadGraph message)
      -> std::unique_ptr<WorkerState<V, E, M>> {
    LOG_TOPIC("cd69c", INFO, Logger::PREGEL)
        << fmt::format("Worker Actor {} is loading", this->self);

    auto newState = this->executionState->receive(
        this->sender, message, dispatchStatus, dispatchMetrics,
        dispatchConductor, dispatchSelf);
    changeState(newState);

    return std::move(this->state);
  }

  // ----- computing -----
  auto operator()(message::RunGlobalSuperStep message)
      -> std::unique_ptr<WorkerState<V, E, M>> {
    LOG_TOPIC("0f658", INFO, Logger::PREGEL)
        << fmt::format("Worker Actor {} is computing", this->self);

    auto newState = this->executionState->receive(
        this->sender, message, dispatchStatus, dispatchMetrics,
        dispatchConductor, dispatchSelf);
    changeState(newState);

    return std::move(this->state);
  }

  auto operator()(message::PregelMessage message)
      -> std::unique_ptr<WorkerState<V, E, M>> {
    auto newState = this->executionState->receive(
        this->sender, message, dispatchStatus, dispatchMetrics,
        dispatchConductor, dispatchSelf);
    changeState(newState);

    return std::move(this->state);
  }

  // ------ end computing ----

  auto operator()([[maybe_unused]] message::Store msg)
      -> std::unique_ptr<WorkerState<V, E, M>> {
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

    this->template dispatch<metrics::message::MetricsMessages>(
        this->state->metricsActor,
        arangodb::pregel::metrics::message::WorkerStoringFinished{});

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

  auto operator()([[maybe_unused]] message::Cleanup msg)
      -> std::unique_ptr<WorkerState<V, E, M>> {
    this->finish();

    LOG_TOPIC("664f5", INFO, Logger::PREGEL)
        << fmt::format("Worker Actor {} is cleaned", this->self);

    this->template dispatch<pregel::message::SpawnMessages>(
        this->state->spawnActor, pregel::message::SpawnCleanup{});
    this->template dispatch<pregel::conductor::message::ConductorMessages>(
        this->state->conductor, pregel::conductor::message::CleanupFinished{});
    this->template dispatch<metrics::message::MetricsMessages>(
        this->state->metricsActor,
        arangodb::pregel::metrics::message::WorkerFinished{});

    changeState(std::make_unique<CleanedUp>());

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

  auto changeState(std::unique_ptr<ExecutionState> newState) -> void {
    if (newState != nullptr) {
      this->state->executionState = std::move(newState);
      LOG_TOPIC("b11f4", INFO, Logger::PREGEL)
          << fmt::format("Worker Actor: Execution state changed to {}",
                         this->state->executionState->name());
    }
  }
};

}  // namespace arangodb::pregel::worker
