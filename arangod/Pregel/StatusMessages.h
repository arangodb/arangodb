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
#include "Inspection/Types.h"

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
  std::string database;
  std::string algorithm;
  TTL ttl;
  size_t parallelism;
};
template<typename Inspector>
auto inspect(Inspector& f, StatusStart& x) {
  return f.object(x).fields(
      f.field("state", x.state), f.field("id", x.id),
      f.field("database", x.database), f.field("algorithm", x.algorithm),
      f.field("ttl", x.ttl), f.field("parallelism", x.parallelism));
}

struct PregelStarted {
  std::string state;
  TimingInMicroseconds time = TimingInMicroseconds::now();
};
template<typename Inspector>
auto inspect(Inspector& f, PregelStarted& x) {
  return f.object(x).fields(f.field("state", x.state), f.field("time", x.time));
}

struct LoadingStarted {
  std::string state;
  TimingInMicroseconds time = TimingInMicroseconds::now();
};
template<typename Inspector>
auto inspect(Inspector& f, LoadingStarted& x) {
  return f.object(x).fields(f.field("state", x.state), f.field("time", x.time));
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
  VPackBuilder aggregators;
  std::string state;
  TimingInMicroseconds time = TimingInMicroseconds::now();
};
template<typename Inspector>
auto inspect(Inspector& f, GlobalSuperStepStarted& x) {
  return f.object(x).fields(f.field("gss", x.gss),
                            f.field("aggregators", x.aggregators),
                            f.field("state", x.state), f.field("time", x.time));
}

struct StoringStarted {
  std::string state;
  TimingInMicroseconds time = TimingInMicroseconds::now();
};
template<typename Inspector>
auto inspect(Inspector& f, StoringStarted& x) {
  return f.object(x).fields(f.field("state", x.state), f.field("time", x.time));
}

struct StatusDone {
  std::string state;
  TimingInMicroseconds time = TimingInMicroseconds::now();
};
template<typename Inspector>
auto inspect(Inspector& f, StatusDone& x) {
  return f.object(x).fields(f.field("state", x.state), f.field("time", x.time));
}

struct StatusMessages : std::variant<StatusStart, PregelStarted, LoadingStarted,
                                     ComputationStarted, GlobalSuperStepStarted,
                                     StoringStarted, StatusDone> {
  using std::variant<StatusStart, PregelStarted, LoadingStarted,
                     ComputationStarted, GlobalSuperStepStarted, StoringStarted,
                     StatusDone>::variant;
};
template<typename Inspector>
auto inspect(Inspector& f, StatusMessages& x) {
  return f.variant(x).unqualified().alternatives(
      arangodb::inspection::type<StatusStart>("Start"),
      arangodb::inspection::type<PregelStarted>("PregelStarted"),
      arangodb::inspection::type<LoadingStarted>("LoadingStarted"),
      arangodb::inspection::type<ComputationStarted>("ComputationStarted"),
      arangodb::inspection::type<GlobalSuperStepStarted>(
          "GlobalSuperStepStarted"),
      arangodb::inspection::type<StoringStarted>("StoringStarted"),
      arangodb::inspection::type<StatusDone>("Done"));
}

}  // namespace arangodb::pregel::message
