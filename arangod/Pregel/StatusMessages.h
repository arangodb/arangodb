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

struct TimingInMilliseconds {
  uint64_t value;
  static auto now() -> TimingInMilliseconds {
    return TimingInMilliseconds{
        .value = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count())};
  }
};
template<typename Inspector>
auto inspect(Inspector& f, TimingInMilliseconds& x) {
  if constexpr (Inspector::isLoading) {
    uint64_t v = 0;
    auto res = f.apply(v);
    if (res.ok()) {
      x = TimingInMilliseconds{.value = v};
    }
    return res;
  } else {
    return f.apply(x.value);
  }
}

struct StatusStart {};
template<typename Inspector>
auto inspect(Inspector& f, StatusStart& x) {
  return f.object(x).fields();
}

struct LoadingStarted {
  std::string state;
  TimingInMilliseconds time = TimingInMilliseconds::now();
};
template<typename Inspector>
auto inspect(Inspector& f, LoadingStarted& x) {
  return f.object(x).fields(f.field("state", x.state), f.field("time", x.time));
}

struct StatusMessages : std::variant<StatusStart, LoadingStarted> {
  using std::variant<StatusStart, LoadingStarted>::variant;
};
template<typename Inspector>
auto inspect(Inspector& f, StatusMessages& x) {
  return f.variant(x).unqualified().alternatives(
      arangodb::inspection::type<StatusStart>("Start"),
      arangodb::inspection::type<LoadingStarted>("LoadingStarted"));
}

}  // namespace arangodb::pregel::message
