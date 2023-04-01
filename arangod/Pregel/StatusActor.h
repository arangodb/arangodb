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
#include "Pregel/StatusMessages.h"
#include "fmt/core.h"

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
    return f.apply(fmt::format("{:.6f} s", x.timing.value / 1000000.)); // NOLINT(cppcoreguidelines-narrowing-conversions)
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
  std::vector<PrintableDuration> gss;
};
template<typename Inspector>
auto inspect(Inspector& f, PregelTimings& x) {
  return f.object(x).fields(f.field("totalRuntime", x.totalRuntime),
                            f.field("loading", x.loading),
                            f.field("gss", x.gss));
}

struct StatusState {
  std::string stateName;
  ExecutionNumber id;
  std::string database;
  std::string algorithm;
  std::optional<PrintableTiming> created;
  TTL ttl;
  size_t parallelism;
  PregelTimings timings;
  uint64_t gss;
  VPackBuilder aggregators;
};
template<typename Inspector>
auto inspect(Inspector& f, StatusState& x) {
  return f.object(x).fields(
      f.field("state", x.stateName), f.field("id", x.id),
      f.field("database", x.database), f.field("algorithm", x.algorithm),
      f.field("created", x.created), f.field("ttl", x.ttl),
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
    this->state->created = PrintableTiming{msg.time};
    this->state->ttl = msg.ttl;
    this->state->parallelism = msg.parallelism;

    LOG_TOPIC("ea4f4", INFO, Logger::PREGEL)
        << fmt::format("Status Actor {} started", this->self);
    return std::move(this->state);
  }

  auto operator()(message::PregelStarted msg) -> std::unique_ptr<StatusState> {
    this->state->stateName = msg.state;
    this->state->timings.totalRuntime.setStart(msg.time);
    return std::move(this->state);
  }

  auto operator()(message::LoadingStarted msg) -> std::unique_ptr<StatusState> {
    this->state->stateName = msg.state;
    this->state->timings.loading.setStart(msg.time);
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

  auto operator()([[maybe_unused]] auto&& rest) -> std::unique_ptr<StatusState> {
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
