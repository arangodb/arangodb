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
  auto operator()([[maybe_unused]] metrics::message::MetricsStart msg) {
    LOG_TOPIC("89eac", INFO, Logger::PREGEL)
        << fmt::format("Metric Actor {} started", this->self);

    return std::move(this->state);
  }

  auto operator()([[maybe_unused]] metrics::message::ConductorStarted msg) {
    this->state->metrics->pregelConductorsNumber->fetch_add(1);

    return std::move(this->state);
  }

  auto operator()([[maybe_unused]] metrics::message::ConductorLoadingStarted msg) {
    this->state->metrics->pregelConductorsLoadingNumber->fetch_add(1);

    return std::move(this->state);
  }

  auto operator()([[maybe_unused]] metrics::message::ConductorComputingStarted msg) {
    this->state->metrics->pregelConductorsLoadingNumber->fetch_sub(1);
    this->state->metrics->pregelConductorsRunningNumber->fetch_add(1);

    return std::move(this->state);
  }

  auto operator()([[maybe_unused]] metrics::message::ConductorStoringStarted msg) {
    this->state->metrics->pregelConductorsRunningNumber->fetch_sub(1);
    this->state->metrics->pregelConductorsStoringNumber->fetch_add(1);

    return std::move(this->state);
  }

  auto operator()(metrics::message::ConductorFinished msg) {
    this->state->metrics->pregelConductorsNumber->fetch_sub(1);

    switch (msg.prevState) {
      case metrics::message::PrevState::LOADING:
        this->state->metrics->pregelConductorsLoadingNumber->fetch_sub(1);
        break;
      case metrics::message::PrevState::COMPUTING:
        this->state->metrics->pregelConductorsRunningNumber->fetch_sub(1);
        break;
      case metrics::message::PrevState::STORING:
        this->state->metrics->pregelConductorsStoringNumber->fetch_sub(1);
        break;
      case metrics::message::PrevState::OTHER:
        break;
    }

    return std::move(this->state);
  }

  auto operator()([[maybe_unused]] metrics::message::WorkerStarted msg) {
    this->state->metrics->pregelWorkersNumber->fetch_add(1);

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

  auto operator()([[maybe_unused]] auto&& rest)
      -> std::unique_ptr<MetricsState> {
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
