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
  PrintableTiming(uint64_t timingInMilliseconds)
      : timing{message::TimingInMilliseconds{.value = timingInMilliseconds}} {}
  PrintableTiming(message::TimingInMilliseconds timingInMilliseconds)
      : timing{timingInMilliseconds} {}
  message::TimingInMilliseconds timing;
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
    // TODO add unit seconds at the end
    return f.apply(fmt::format("{:.4f} s", x.timing.value / 1000.));
  }
}
struct PrintableDuration {
  std::optional<PrintableTiming> start;
  std::optional<PrintableTiming> stop;
  auto setStart(message::TimingInMilliseconds timing) -> void {
    if (not start.has_value()) {
      start = PrintableTiming{timing};
    }
  }
  auto duration() const -> PrintableTiming {
    if (not start.has_value()) {
      return 0;
    }
    if (not stop.has_value()) {
      return PrintableTiming{message::TimingInMilliseconds::now().value -
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
  PrintableDuration loading;
};
template<typename Inspector>
auto inspect(Inspector& f, PregelTimings& x) {
  return f.object(x).fields(f.field("loading", x.loading));
}

struct StatusState {
  std::string stateName;
  PregelTimings timings;
};
template<typename Inspector>
auto inspect(Inspector& f, StatusState& x) {
  return f.object(x).fields(f.field("state", x.stateName),
                            f.field("timings", x.timings));
}

template<typename Runtime>
struct StatusHandler : actor::HandlerBase<Runtime, StatusState> {
  auto operator()(message::StatusStart start) -> std::unique_ptr<StatusState> {
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

  auto operator()(auto&& rest) -> std::unique_ptr<StatusState> {
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
