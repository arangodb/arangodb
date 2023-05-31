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
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <chrono>
#include <variant>
#include "Actor/ActorPID.h"
#include "Pregel/PregelOptions.h"
#include "Inspection/Types.h"
#include "MetricsMessages.h"

namespace arangodb::pregel::message {

struct TimingInMicroseconds {
  uint64_t value;
  static auto now() -> TimingInMicroseconds {
    return TimingInMicroseconds{
        .value = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count())};
  }
  static auto systemNow() -> TimingInMicroseconds {
    return TimingInMicroseconds{
        .value = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count())};
  }
};
template<typename Inspector>
auto inspect(Inspector& f, TimingInMicroseconds& x) {
  if constexpr (Inspector::isLoading) {
    uint64_t v = 0;
    auto res = f.apply(v);
    if (res.ok()) {
      x = TimingInMicroseconds{.value = v};
    }
    return res;
  } else {
    return f.apply(x.value);
  }
}

struct StatusStart {
  std::string state;
  ExecutionNumber id;
  std::string user;
  std::string database;
  std::string algorithm;
  TTL ttl;
  size_t parallelism;
};
template<typename Inspector>
auto inspect(Inspector& f, StatusStart& x) {
  return f.object(x).fields(
      f.field("state", x.state), f.field("id", x.id), f.field("user", x.user),
      f.field("database", x.database), f.field("algorithm", x.algorithm),
      f.field("ttl", x.ttl), f.field("parallelism", x.parallelism));
}

struct PregelStarted {
  std::string state;
  // we need two timings here:
  // time is used to measure the pregel run duration (based on the steady_clock)
  // systemTime is used to set the created datetime (based on the system_clock)
  TimingInMicroseconds time = TimingInMicroseconds::now();
  TimingInMicroseconds systemTime = TimingInMicroseconds::systemNow();
};
template<typename Inspector>
auto inspect(Inspector& f, PregelStarted& x) {
  return f.object(x).fields(f.field("state", x.state), f.field("time", x.time),
                            f.field("systemTime", x.systemTime));
}

struct LoadingStarted {
  std::string state;
  TimingInMicroseconds time = TimingInMicroseconds::now();
};
template<typename Inspector>
auto inspect(Inspector& f, LoadingStarted& x) {
  return f.object(x).fields(f.field("state", x.state), f.field("time", x.time));
}

struct GraphLoadingUpdate {
  std::uint64_t verticesLoaded;
  std::uint64_t edgesLoaded;
  std::uint64_t memoryBytesUsed;
};
template<typename Inspector>
auto inspect(Inspector& f, GraphLoadingUpdate& x) {
  return f.object(x).fields(f.field("verticesLoaded", x.verticesLoaded),
                            f.field("edgesLoaded", x.edgesLoaded),
                            f.field("memoryBytesUsed", x.memoryBytesUsed));
}

struct ComputationStarted {
  std::string state;
  TimingInMicroseconds time = TimingInMicroseconds::now();
};
template<typename Inspector>
auto inspect(Inspector& f, ComputationStarted& x) {
  return f.object(x).fields(f.field("state", x.state), f.field("time", x.time));
}

struct GlobalSuperStepStarted {
  uint64_t gss;
  uint64_t vertexCount;
  uint64_t edgeCount;
  VPackBuilder aggregators;
  std::string state;
  TimingInMicroseconds time = TimingInMicroseconds::now();
};
template<typename Inspector>
auto inspect(Inspector& f, GlobalSuperStepStarted& x) {
  return f.object(x).fields(
      f.field("gss", x.gss), f.field("vertexCount", x.vertexCount),
      f.field("edgeCount", x.edgeCount), f.field("aggregators", x.aggregators),
      f.field("state", x.state), f.field("time", x.time));
}

struct GlobalSuperStepUpdate {
  std::uint64_t gss;
  std::uint64_t verticesProcessed = 0;
  std::uint64_t messagesSent = 0;
  std::uint64_t messagesReceived = 0;
  std::uint64_t memoryBytesUsedForMessages = 0;
};
template<typename Inspector>
auto inspect(Inspector& f, GlobalSuperStepUpdate& x) {
  return f.object(x).fields(
      f.field("gss", x.gss), f.field("verticesProcessed", x.verticesProcessed),
      f.field("messagesSent", x.messagesSent),
      f.field("messagesReceived", x.messagesReceived),
      f.field("memoryBytesUsedForMessages", x.memoryBytesUsedForMessages));
}

struct StoringStarted {
  std::string state;
  TimingInMicroseconds time = TimingInMicroseconds::now();
};
template<typename Inspector>
auto inspect(Inspector& f, StoringStarted& x) {
  return f.object(x).fields(f.field("state", x.state), f.field("time", x.time));
}

struct GraphStoringUpdate {
  uint64_t verticesStored;
};
template<typename Inspector>
auto inspect(Inspector& f, GraphStoringUpdate& x) {
  return f.object(x).fields(f.field("verticesStored", x.verticesStored));
}

struct PregelFinished {
  std::string state;
  TimingInMicroseconds time = TimingInMicroseconds::now();
};
template<typename Inspector>
auto inspect(Inspector& f, PregelFinished& x) {
  return f.object(x).fields(f.field("state", x.state), f.field("time", x.time));
}

struct InFatalError {
  std::string state;
  std::string errorMessage;
  TimingInMicroseconds time = TimingInMicroseconds::now();
};
template<typename Inspector>
auto inspect(Inspector& f, InFatalError& x) {
  return f.object(x).fields(f.field("state", x.state),
                            f.field("errorMessage", x.errorMessage),
                            f.field("time", x.time));
}

struct Canceled {
  std::string state;
  TimingInMicroseconds time = TimingInMicroseconds::now();
};
template<typename Inspector>
auto inspect(Inspector& f, Canceled& x) {
  return f.object(x).fields(f.field("state", x.state), f.field("time", x.time));
}

struct Cleanup {};
template<typename Inspector>
auto inspect(Inspector& f, Cleanup& x) {
  return f.object(x).fields();
}

struct StatusMessages
    : std::variant<StatusStart, PregelStarted, LoadingStarted,
                   GraphLoadingUpdate, ComputationStarted,
                   GlobalSuperStepStarted, GlobalSuperStepUpdate,
                   StoringStarted, GraphStoringUpdate, PregelFinished,
                   InFatalError, Canceled, Cleanup> {
  using std::variant<StatusStart, PregelStarted, LoadingStarted,
                     GraphLoadingUpdate, ComputationStarted,
                     GlobalSuperStepStarted, GlobalSuperStepUpdate,
                     StoringStarted, GraphStoringUpdate, PregelFinished,
                     InFatalError, Canceled, Cleanup>::variant;
};
template<typename Inspector>
auto inspect(Inspector& f, StatusMessages& x) {
  return f.variant(x).unqualified().alternatives(
      arangodb::inspection::type<StatusStart>("Start"),
      arangodb::inspection::type<PregelStarted>("PregelStarted"),
      arangodb::inspection::type<LoadingStarted>("LoadingStarted"),
      arangodb::inspection::type<GraphLoadingUpdate>("GraphLoadingUpdate"),
      arangodb::inspection::type<ComputationStarted>("ComputationStarted"),
      arangodb::inspection::type<GlobalSuperStepStarted>(
          "GlobalSuperStepStarted"),
      arangodb::inspection::type<GlobalSuperStepUpdate>(
          "GlobalSuperStepUpdate"),
      arangodb::inspection::type<StoringStarted>("StoringStarted"),
      arangodb::inspection::type<GraphStoringUpdate>("GraphStoringUpdate"),
      arangodb::inspection::type<PregelFinished>("PregelFinished"),
      arangodb::inspection::type<InFatalError>("InFatalError"),
      arangodb::inspection::type<Canceled>("Canceled"),
      arangodb::inspection::type<Cleanup>("Cleanup"));
}

}  // namespace arangodb::pregel::message
