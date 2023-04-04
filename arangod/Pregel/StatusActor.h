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

struct GraphLoadingDetails {
  auto add(GraphLoadingDetails const& other) -> void {
    verticesLoaded += other.verticesLoaded;
    edgesLoaded += other.edgesLoaded;
    memoryBytesUsed += other.memoryBytesUsed;
  }
  std::uint64_t verticesLoaded = 0;
  std::uint64_t edgesLoaded = 0;
  std::uint64_t memoryBytesUsed = 0;
};
template<typename Inspector>
auto inspect(Inspector& f, GraphLoadingDetails& x) {
  return f.object(x).fields(f.field("verticesLoaded", x.verticesLoaded),
                            f.field("edgesLoaded", x.edgesLoaded),
                            f.field("memoryBytesUsed", x.memoryBytesUsed));
}
struct GraphStoringDetails {
  auto add(GraphStoringDetails const& other) -> void {
    verticesStored += other.verticesStored;
  }
  uint64_t verticesStored = 0;
};
template<typename Inspector>
auto inspect(Inspector& f, GraphStoringDetails& x) {
  return f.object(x).fields(f.field("verticesStored", x.verticesStored));
}
struct GlobalSuperStepDetails {
  auto add(GlobalSuperStepDetails const& other) -> void {
    verticesProcessed += other.verticesProcessed;
    messagesSent += other.messagesSent;
    messagesReceived += other.messagesReceived;
    memoryBytesUsedForMessages += other.memoryBytesUsedForMessages;
  }
  uint64_t verticesProcessed = 0;
  uint64_t messagesSent = 0;
  uint64_t messagesReceived = 0;
  uint64_t memoryBytesUsedForMessages = 0;
};
template<typename Inspector>
auto inspect(Inspector& f, GlobalSuperStepDetails& x) {
  return f.object(x).fields(
      f.field("verticesProcessed", x.verticesProcessed),
      f.field("messagesSent", x.messagesSent),
      f.field("messagesReceived", x.messagesReceived),
      f.field("memoryBytesUsedForMessages", x.memoryBytesUsedForMessages));
}

struct Details {
  GraphLoadingDetails loading;
  GraphStoringDetails storing;
  std::unordered_map<std::string, GlobalSuperStepDetails> computing;
};
template<typename Inspector>
auto inspect(Inspector& f, Details& x) {
  return f.object(x).fields(f.field("graphLoading", x.loading),
                            f.field("computing", x.computing),
                            f.field("graphStoring", x.storing));
}
struct StatusDetails {
  auto update(ServerID server, GraphLoadingDetails const& loadingDetails)
      -> void {
    perWorker[server].loading = loadingDetails;
    // update combined
    GraphLoadingDetails loadingCombined;
    for (auto& [server, details] : perWorker) {
      loadingCombined.add(details.loading);
    }
    combined.loading = loadingCombined;
  }
  auto update(ServerID server, GraphStoringDetails const& storingDetails)
      -> void {
    perWorker[server].storing = storingDetails;
    // update combined
    GraphStoringDetails storingCombined;
    for (auto& [server, details] : perWorker) {
      storingCombined.add(details.storing);
    }
    combined.storing = storingCombined;
  }
  auto update(ServerID server, uint64_t gss,
              GlobalSuperStepDetails const& gssDetails) -> void {
    auto gssName = fmt::format("gss_{}", gss);
    perWorker[server].computing[gssName] = gssDetails;
    // update combined
    GlobalSuperStepDetails lastGssCombined;
    for (auto& [server, details] : perWorker) {
      auto detailsForGss = details.computing.contains(gssName)
                               ? details.computing[gssName]
                               : GlobalSuperStepDetails{};
      lastGssCombined.add(detailsForGss);
    }
    combined.computing[gssName] = lastGssCombined;
  }

  std::unordered_map<ServerID, Details> perWorker;
  Details combined;
};
template<typename Inspector>
auto inspect(Inspector& f, StatusDetails& x) {
  return f.object(x).fields(f.field("total", x.combined),
                            f.field("perWorker", x.perWorker));
}
struct StatusState {
  std::string stateName;
  PregelTimings timings;
  uint64_t vertexCount;
  uint64_t edgeCount;
  StatusDetails details;
};
template<typename Inspector>
auto inspect(Inspector& f, StatusState& x) {
  return f.object(x).fields(
      f.field("stateName", x.stateName), f.field("timings", x.timings),
      f.field("vertexCount", x.vertexCount), f.field("edgeCount", x.edgeCount),
      f.field("details", x.details));
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
