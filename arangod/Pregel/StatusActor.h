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
#include "Actor/ActorPID.h"
#include "Actor/HandlerBase.h"
#include "Inspection/Status.h"
#include "Inspection/Format.h"
#include "Pregel/DatabaseTypes.h"
#include "Pregel/StatusMessages.h"
#include "fmt/core.h"
#include "fmt/chrono.h"

namespace arangodb::pregel {

struct PrintableTiming {
  explicit PrintableTiming(uint64_t timingInMicroseconds)
      : timing{message::TimingInMicroseconds{.value = timingInMicroseconds}} {}
  explicit PrintableTiming(message::TimingInMicroseconds timingInMicroseconds)
      : timing{timingInMicroseconds} {}
  message::TimingInMicroseconds timing;
};
template<typename Inspector>
auto inspect(Inspector& f, PrintableTiming& x) {
  if constexpr (Inspector::isLoading) {
    uint64_t v = 0;
    auto res = f.apply(v);
    if (res.ok()) {
      x = PrintableTiming{v};
    }
    return res;
  } else {
    return f.apply(fmt::format("{:.6f} s", x.timing.value / 1000000.));
  }
}
struct PrintableDuration {
  std::optional<PrintableTiming> start;
  std::optional<PrintableTiming> stop;
  static auto withStart(message::TimingInMicroseconds timing)
      -> PrintableDuration {
    auto duration = PrintableDuration{};
    duration.setStart(timing);
    return duration;
  }
  auto setStart(message::TimingInMicroseconds timing) -> void {
    if (not start.has_value()) {
      start = PrintableTiming{timing};
    }
  }
  auto setStop(message::TimingInMicroseconds timing) -> void {
    if (not stop.has_value()) {
      stop = PrintableTiming{timing};
    }
  }
  [[nodiscard]] auto duration() const -> PrintableTiming {
    if (not start.has_value()) {
      return PrintableTiming{0};
    }
    if (not stop.has_value()) {
      return PrintableTiming{message::TimingInMicroseconds::now().value -
                             start.value().timing.value};
    }
    return PrintableTiming{stop.value().timing.value -
                           start.value().timing.value};
  }
};
template<typename Inspector>
auto inspect(Inspector& f, PrintableDuration& x) {
  if constexpr (Inspector::isLoading) {
    return inspection::Status::Success{};
  } else {
    return f.apply(x.duration());
  }
}
struct PregelTimings {
  PrintableDuration totalRuntime;
  PrintableDuration loading;
  PrintableDuration computation;
  PrintableDuration storing;
  std::vector<PrintableDuration> gss;

  auto stopAll(message::TimingInMicroseconds timing) {
    totalRuntime.setStop(timing);
    loading.setStop(timing);
    computation.setStop(timing);
    storing.setStop(timing);

    for (auto& g : gss) {
      g.setStop(timing);
    }
  }
};
template<typename Inspector>
auto inspect(Inspector& f, PregelTimings& x) {
  return f.object(x).fields(
      f.field("totalRuntime", x.totalRuntime), f.field("loading", x.loading),
      f.field("computation", x.computation), f.field("storing", x.storing),
      f.field("gss", x.gss));
}

struct StatusState {
  std::string stateName;
  ExecutionNumber id;
  std::string database;
  std::string algorithm;
  std::chrono::steady_clock::time_point created;
  std::optional<std::chrono::steady_clock::time_point> expires;
  TTL ttl;
  size_t parallelism;
  PregelTimings timings;
  uint64_t gss;
  VPackBuilder aggregators;
};
template<typename Inspector>
auto inspect(Inspector& f, StatusState& x) {
  // TODO: Fix formatting.
  auto formatted_created = fmt::format("{}", x.created.time_since_epoch());
  auto formatted_expires =
      x.expires.has_value()
          ? fmt::format("{}", x.expires.value().time_since_epoch())
          : "0";

  return f.object(x).fields(
      f.field("state", x.stateName), f.field("id", x.id),
      f.field("database", x.database), f.field("algorithm", x.algorithm),
      f.field("created", formatted_created),
      f.field("expires", formatted_expires), f.field("ttl", x.ttl),
      f.field("parallelism", x.parallelism), f.field("timings", x.timings),
      f.field("gss", x.gss),
      // TODO embed aggregators field (it already has "aggregators" as key)
      f.field("aggregators", x.aggregators));
}

template<typename Runtime>
struct StatusHandler : actor::HandlerBase<Runtime, StatusState> {
  auto operator()(message::StatusStart& msg) -> std::unique_ptr<StatusState> {
    this->state->stateName = msg.state;
    this->state->id = msg.id;
    this->state->database = msg.database;
    this->state->algorithm = msg.algorithm;
    this->state->ttl = msg.ttl;
    this->state->parallelism = msg.parallelism;

    LOG_TOPIC("ea4f4", INFO, Logger::PREGEL)
        << fmt::format("Status Actor {} started", this->self);
    return std::move(this->state);
  }

  auto operator()(message::LoadingStarted loading)
      -> std::unique_ptr<StatusState> {
    this->state->stateName = loading.state;
    this->state->timings.loading.setStart(loading.time);
    return std::move(this->state);
  }
  auto operator()(message::GraphLoadingUpdate msg)
      -> std::unique_ptr<StatusState> {
    this->state->details.update(
        this->sender.server,
        GraphLoadingDetails{.verticesLoaded = msg.verticesLoaded,
                            .edgesLoaded = msg.edgesLoaded,
                            .memoryBytesUsed = msg.memoryBytesUsed});
    return std::move(this->state);
  }

  auto operator()(message::GlobalSuperStepUpdate msg)
      -> std::unique_ptr<StatusState> {
    this->state->details.update(
        this->sender.server, msg.gss,
        GlobalSuperStepDetails{
            .verticesProcessed = msg.verticesProcessed,
            .messagesSent = msg.messagesSent,
            .messagesReceived = msg.messagesReceived,
            .memoryBytesUsedForMessages = msg.memoryBytesUsedForMessages});
    return std::move(this->state);
  }

  auto operator()(message::GraphStoringUpdate msg)
      -> std::unique_ptr<StatusState> {
    this->state->details.update(
        this->sender.server,
        GraphStoringDetails{.verticesStored = msg.verticesStored});
    return std::move(this->state);
  }

  auto operator()(message::PregelStarted msg) -> std::unique_ptr<StatusState> {
    this->state->stateName = msg.state;
    this->state->timings.totalRuntime.setStart(msg.time);

    auto duration = std::chrono::microseconds{msg.time.value};
    this->state->created =
        std::chrono::time_point<std::chrono::steady_clock>{duration};

    return std::move(this->state);
  }

  auto operator()(message::LoadingStarted msg) -> std::unique_ptr<StatusState> {
    this->state->stateName = msg.state;
    this->state->timings.loading.setStart(msg.time);
    return std::move(this->state);
  }

  auto operator()(message::ComputationStarted msg)
      -> std::unique_ptr<StatusState> {
    this->state->stateName = msg.state;
    this->state->timings.loading.setStop(msg.time);
    this->state->timings.computation.setStart(msg.time);
    this->state->timings.gss.push_back(PrintableDuration::withStart(msg.time));
    return std::move(this->state);
  }

  auto operator()(message::StoringStarted msg) -> std::unique_ptr<StatusState> {
    this->state->stateName = msg.state;
    this->state->timings.computation.setStop(msg.time);

    if (not this->state->timings.gss.empty()) {
      this->state->timings.gss.back().setStop(msg.time);
    }

    this->state->timings.storing.setStart(msg.time);

    return std::move(this->state);
  }

  auto operator()(message::GlobalSuperStepStarted msg)
      -> std::unique_ptr<StatusState> {
    this->state->stateName = msg.state;
    this->state->gss = msg.gss;
    if (not this->state->timings.gss.empty()) {
      this->state->timings.gss.back().setStop(msg.time);
    }
    this->state->timings.gss.push_back(PrintableDuration::withStart(msg.time));
    this->state->aggregators = std::move(msg.aggregators);
    return std::move(this->state);
  }

  auto operator()(message::PregelFinished& msg)
      -> std::unique_ptr<StatusState> {
    this->state->stateName = msg.state;
    this->state->expires =
        std::chrono::steady_clock::now() + this->state->ttl.duration;
    this->state->timings.storing.setStop(msg.time);
    this->state->timings.totalRuntime.setStop(msg.time);

    return std::move(this->state);
  }

  auto operator()(message::InFatalError& msg) -> std::unique_ptr<StatusState> {
    this->state->stateName = msg.state;
    this->state->timings.stopAll(msg.time);

    return std::move(this->state);
  }

  auto operator()(message::Canceled& msg) -> std::unique_ptr<StatusState> {
    this->state->stateName = msg.state;
    this->state->timings.stopAll(msg.time);

    return std::move(this->state);
  }

  auto operator()(actor::message::UnknownMessage unknown)
      -> std::unique_ptr<StatusState> {
    LOG_TOPIC("eb6f2", INFO, Logger::PREGEL) << fmt::format(
        "Status Actor: Error - sent unknown message to {}", unknown.receiver);
    return std::move(this->state);
  }

  auto operator()(actor::message::ActorNotFound notFound)
      -> std::unique_ptr<StatusState> {
    LOG_TOPIC("e31f6", INFO, Logger::PREGEL) << fmt::format(
        "Status Actor: Error - receiving actor {} not found", notFound.actor);
    return std::move(this->state);
  }

  auto operator()(actor::message::NetworkError notFound)
      -> std::unique_ptr<StatusState> {
    LOG_TOPIC("e87f3", INFO, Logger::PREGEL) << fmt::format(
        "Status Actor: Error - network error {}", notFound.message);
    return std::move(this->state);
  }

  auto operator()([[maybe_unused]] auto&& rest)
      -> std::unique_ptr<StatusState> {
    LOG_TOPIC("e9df2", INFO, Logger::PREGEL)
        << "Status Actor: Got unhandled message";
    return std::move(this->state);
  }
};

struct StatusActor {
  using State = StatusState;
  using Message = message::StatusMessages;
  template<typename Runtime>
  using Handler = StatusHandler<Runtime>;
  static constexpr auto typeName() -> std::string_view {
    return "Status Actor";
  }
};

}  // namespace arangodb::pregel
