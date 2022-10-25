////////////////////////////////////////////////////////////////////////////////
///
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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <vector>
#include <memory>
#include <deque>

#include "Pid.h"
#include "Message.h"
#include "RuntimeActor.h"

namespace arangodb::actor_framework {

struct Runtime {
  template<typename T>
  auto spawn() -> PidT {
    auto pid = PidT{current_pid++};

    actors.emplace_back(std::make_unique<T>(pid, *this));

    return pid;
  }

  auto send(PidT from, PidT to, std::unique_ptr<MsgPayloadBase> payload)
      -> void {
    // TODO: error handling
    actors[to.pid].inbox.emplace_back(from, std::move(payload));
  }

  auto run() -> void;

  size_t current_pid{0};

  std::deque<RuntimeActor> actors;
};
}  // namespace arangodb::actor_framework
