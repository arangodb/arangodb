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

#pragma once

#include "Actor/HandlerBase.h"
#include "Pregel/MetricsMessages.h"
#include "Pregel/PregelMetrics.h"
#include "Logger/LogMacros.h"

namespace arangodb::pregel {
struct MetricsState {
  explicit MetricsState(const std::shared_ptr<PregelMetrics>& metrics)
      : metrics(metrics) {}

  std::shared_ptr<PregelMetrics> metrics;
};
template<typename Inspector>
auto inspect(Inspector& f, MetricsState& x) {
  // TODO: Implement this?
  return f.object(x).fields();
}

template<typename Runtime>
struct MetricsHandler : actor::HandlerBase<Runtime, MetricsState> {
  auto operator()(metrics::message::MetricsStart msg) {
    LOG_TOPIC("89eac", INFO, Logger::PREGEL)
        << fmt::format("Metric Actor {} started", this->self);

    return std::move(this->state);
  }

  auto operator()(metrics::message::ConductorStarted msg) {
    this->state->metrics->pregelConductorsNumber->fetch_add(1);

    return std::move(this->state);
  }

  auto operator()(metrics::message::ConductorLoadingStarted msg) {
    this->state->metrics->pregelConductorsLoadingNumber->fetch_add(1);

    return std::move(this->state);
  }

  auto operator()(metrics::message::ConductorComputingStarted msg) {
    this->state->metrics->pregelConductorsLoadingNumber->fetch_sub(1);
    this->state->metrics->pregelConductorsRunningNumber->fetch_add(1);

    return std::move(this->state);
  }

  auto operator()(metrics::message::ConductorStoringStarted msg) {
    this->state->metrics->pregelConductorsRunningNumber->fetch_sub(1);
    this->state->metrics->pregelConductorsStoringNumber->fetch_add(1);

    return std::move(this->state);
  }

  auto operator()(metrics::message::ConductorFinished msg) {
    this->state->metrics->pregelConductorsNumber->fetch_sub(1);

    switch (msg.previousState) {
      case metrics::message::PreviousState::LOADING:
        this->state->metrics->pregelConductorsLoadingNumber->fetch_sub(1);
        break;
      case metrics::message::PreviousState::COMPUTING:
        this->state->metrics->pregelConductorsRunningNumber->fetch_sub(1);
        break;
      case metrics::message::PreviousState::STORING:
        this->state->metrics->pregelConductorsStoringNumber->fetch_sub(1);
        break;
      case metrics::message::PreviousState::OTHER:
        break;
    }

    this->finish();

    return std::move(this->state);
  }

  auto operator()(metrics::message::WorkerStarted msg) {
    this->state->metrics->pregelWorkersNumber->fetch_add(1);

    return std::move(this->state);
  }

  auto operator()(metrics::message::WorkerLoadingStarted msg) {
    this->state->metrics->pregelWorkersLoadingNumber->fetch_add(1);

    return std::move(this->state);
  }

  auto operator()(metrics::message::WorkerLoadingFinished msg) {
    this->state->metrics->pregelWorkersLoadingNumber->fetch_sub(1);

    return std::move(this->state);
  }

  auto operator()(metrics::message::WorkerGssStarted msg) {
    this->state->metrics->pregelWorkersRunningNumber->fetch_add(1);
    this->state->metrics->pregelNumberOfThreads->fetch_add(msg.threadsAdded);

    return std::move(this->state);
  }

  auto operator()(metrics::message::WorkerGssFinished msg) {
    this->state->metrics->pregelWorkersRunningNumber->fetch_sub(1);
    this->state->metrics->pregelNumberOfThreads->fetch_sub(msg.threadsRemoved);
    this->state->metrics->pregelMessagesSent->count(msg.messagesSent);
    this->state->metrics->pregelMessagesReceived->count(msg.messagesReceived);

    return std::move(this->state);
  }

  auto operator()(metrics::message::WorkerStoringStarted msg) {
    this->state->metrics->pregelWorkersStoringNumber->fetch_add(1);

    return std::move(this->state);
  }

  auto operator()(metrics::message::WorkerStoringFinished msg) {
    this->state->metrics->pregelWorkersStoringNumber->fetch_sub(1);

    return std::move(this->state);
  }

  auto operator()(metrics::message::WorkerFinished msg) {
    this->state->metrics->pregelWorkersNumber->fetch_sub(1);

    return std::move(this->state);
  }

  auto operator()(actor::message::UnknownMessage unknown)
      -> std::unique_ptr<MetricsState> {
    LOG_TOPIC("edc16", INFO, Logger::PREGEL) << fmt::format(
        "Metrics Actor: Error - sent unknown message to {}", unknown.receiver);
    return std::move(this->state);
  }

  auto operator()(actor::message::ActorNotFound notFound)
      -> std::unique_ptr<MetricsState> {
    LOG_TOPIC("c944d", INFO, Logger::PREGEL) << fmt::format(
        "Metrics Actor: Error - receiving actor {} not found", notFound.actor);
    return std::move(this->state);
  }

  auto operator()(actor::message::NetworkError notFound)
      -> std::unique_ptr<MetricsState> {
    LOG_TOPIC("498b1", INFO, Logger::PREGEL) << fmt::format(
        "Metrics Actor: Error - network error {}", notFound.message);
    return std::move(this->state);
  }

  auto operator()(auto&& rest) -> std::unique_ptr<MetricsState> {
    LOG_TOPIC("613ba", INFO, Logger::PREGEL)
        << "Metrics Actor: Got unhandled message";
    return std::move(this->state);
  }
};

struct MetricsActor {
  using State = MetricsState;
  using Message = metrics::message::MetricsMessages;

  template<typename Runtime>
  using Handler = MetricsHandler<Runtime>;

  static constexpr auto typeName() -> std::string_view {
    return "Metrics Actor";
  }
};
}  // namespace arangodb::pregel
