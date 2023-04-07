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
  std::shared_ptr<PregelMetrics> metrics;
};

template<typename Runtime>
struct MetricsHandler : actor::HandlerBase<Runtime, MetricsState> {
  auto operator()([[maybe_unused]] message::MetricsStart msg) {
    LOG_TOPIC("89eac", INFO, Logger::PREGEL)
        << fmt::format("Metric Actor {} started", this->self);

    return std::move(this->state);
  }

  template<message::Gauge G, message::GaugeOp O>
  auto operator()(message::GaugeUpdate<G, O> gauge)
      -> std::unique_ptr<MetricsState> {
    std::visit(
        arangodb::overload{
            // INCR
            [this](message::GaugeUpdate<message::Gauge::CONDUCTORS_TOTAL,
                                        message::GaugeOp::INCR>
                       g) {
              this->state->metrics->pregelConductorsNumber->fetch_add(g.amount);
            },
            [this](message::GaugeUpdate<message::Gauge::CONDUCTORS_LOADING,
                                        message::GaugeOp::INCR>
                       g) {
              this->state->metrics->pregelConductorsLoadingNumber->fetch_add(
                  g.amount);
            },
            [this](message::GaugeUpdate<message::Gauge::CONDUCTORS_RUNNING,
                                        message::GaugeOp::INCR>
                       g) {
              this->state->metrics->pregelConductorsRunningNumber->fetch_add(
                  g.amount);
            },
            [this](message::GaugeUpdate<message::Gauge::CONDUCTORS_STORING,
                                        message::GaugeOp::INCR>
                       g) {
              this->state->metrics->pregelConductorsStoringNumber->fetch_add(
                  g.amount);
            },
            [this](message::GaugeUpdate<message::Gauge::WORKERS_TOTAL,
                                        message::GaugeOp::INCR>
                       g) {
              this->state->metrics->pregelWorkersNumber->fetch_add(g.amount);
            },
            [this](message::GaugeUpdate<message::Gauge::WORKERS_LOADING,
                                        message::GaugeOp::INCR>
                       g) {
              this->state->metrics->pregelWorkersLoadingNumber->fetch_add(
                  g.amount);
            },
            [this](message::GaugeUpdate<message::Gauge::WORKERS_RUNNING,
                                        message::GaugeOp::INCR>
                       g) {
              this->state->metrics->pregelWorkersRunningNumber->fetch_add(
                  g.amount);
            },
            [this](message::GaugeUpdate<message::Gauge::WORKERS_STORING,
                                        message::GaugeOp::INCR>
                       g) {
              this->state->metrics->pregelWorkersStoringNumber->fetch_add(
                  g.amount);
            },
            [this](message::GaugeUpdate<message::Gauge::THREAD_COUNT,
                                        message::GaugeOp::INCR>
                       g) {
              this->state->metrics->pregelNumberOfThreads->fetch_add(g.amount);
            },
            [this](message::GaugeUpdate<message::Gauge::MEMORY_FOR_GRAPH,
                                        message::GaugeOp::INCR>
                       g) {
              this->state->metrics->pregelMemoryUsedForGraph->fetch_add(
                  g.amount);
            },

            // DECR
            [this](message::GaugeUpdate<message::Gauge::CONDUCTORS_TOTAL,
                                        message::GaugeOp::DECR>
                       g) {
              this->state->metrics->pregelConductorsNumber->fetch_sub(g.amount);
            },
            [this](message::GaugeUpdate<message::Gauge::CONDUCTORS_LOADING,
                                        message::GaugeOp::DECR>
                       g) {
              this->state->metrics->pregelConductorsLoadingNumber->fetch_sub(
                  g.amount);
            },
            [this](message::GaugeUpdate<message::Gauge::CONDUCTORS_RUNNING,
                                        message::GaugeOp::DECR>
                       g) {
              this->state->metrics->pregelConductorsRunningNumber->fetch_sub(
                  g.amount);
            },
            [this](message::GaugeUpdate<message::Gauge::CONDUCTORS_STORING,
                                        message::GaugeOp::DECR>
                       g) {
              this->state->metrics->pregelConductorsStoringNumber->fetch_sub(
                  g.amount);
            },
            [this](message::GaugeUpdate<message::Gauge::WORKERS_TOTAL,
                                        message::GaugeOp::DECR>
                       g) {
              this->state->metrics->pregelWorkersNumber->fetch_sub(g.amount);
            },
            [this](message::GaugeUpdate<message::Gauge::WORKERS_LOADING,
                                        message::GaugeOp::DECR>
                       g) {
              this->state->metrics->pregelWorkersLoadingNumber->fetch_sub(
                  g.amount);
            },
            [this](message::GaugeUpdate<message::Gauge::WORKERS_RUNNING,
                                        message::GaugeOp::DECR>
                       g) {
              this->state->metrics->pregelWorkersRunningNumber->fetch_sub(
                  g.amount);
            },
            [this](message::GaugeUpdate<message::Gauge::WORKERS_STORING,
                                        message::GaugeOp::DECR>
                       g) {
              this->state->metrics->pregelWorkersStoringNumber->fetch_sub(
                  g.amount);
            },
            [this](message::GaugeUpdate<message::Gauge::THREAD_COUNT,
                                        message::GaugeOp::DECR>
                       g) {
              this->state->metrics->pregelNumberOfThreads->fetch_sub(g.amount);
            },
            [this](message::GaugeUpdate<message::Gauge::MEMORY_FOR_GRAPH,
                                        message::GaugeOp::DECR>
                       g) {
              this->state->metrics->pregelMemoryUsedForGraph->fetch_sub(
                  g.amount);
            },
        },
        gauge);

    return std::move(this->state);
  }

  template<message::Counter C>
  auto operator()(message::CounterUpdate<C> counter)
      -> std::unique_ptr<MetricsState> {
    std::visit(
        arangodb::overload{
            [this](message::CounterUpdate<message::Counter::MESSAGES_SENT> c) {
              this->state->metrics->pregelMessagesSent->count(c.amount);
            },
            [this](
                message::CounterUpdate<message::Counter::MESSAGES_RECEIVED> c) {
              this->state->metrics->pregelMessagesReceived->count(c.amount);
            }},
        counter);

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
  using Message = message::MetricsMessages;

  template<typename Runtime>
  using Handler = MetricsHandler<Runtime>;

  static constexpr auto typeName() -> std::string_view {
    return "Metrics Actor";
  }
};
}  // namespace arangodb::pregel
