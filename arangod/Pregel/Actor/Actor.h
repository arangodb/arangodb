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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <Inspection/VPackWithErrorT.h>
#include <memory>

#include "MPSCQueue.h"

namespace arangodb::pregel::actor {

struct ActorMessageBase {
  virtual ~ActorMessageBase() = default;
};

struct ActorBase {
  virtual ~ActorBase() = default;
};

template<typename Scheduler, typename MessageHandler, typename State>
struct Actor : ActorBase {
  Actor(Scheduler& schedule, std::unique_ptr<State> initialState)
      : schedule(schedule), state(std::move(initialState)) {}

  Actor(Scheduler& schedule, std::unique_ptr<State> initialState,
        std::size_t batchSize)
      : batchSize(batchSize), schedule(schedule) {}

  void process(std::unique_ptr<ActorMessageBase> msg) {
    inbox.push(std::move(msg));
    kick();
  }

  void kick() {
    // Make sure that *someone* works here
    schedule([this]() { this->work(); });
  }

  void work() {
    auto was_false = false;
    if (busy.compare_exchange_strong(was_false, true)) {
      auto i = batchSize;

      while (auto msg = inbox.pop()) {
        state = std::visit(MessageHandler{std::move(state)}, *msg);
        i--;
        if (i == 0) {
          break;
        }
      }
      busy.store(false);

      if (!inbox.empty()) {
        kick();
      }
    }
  }
  std::size_t batchSize{16};
  std::atomic<bool> busy;
  arangodb::pregel::mpscqueue::MPSCQueue<ActorMessageBase> inbox;
  Scheduler& schedule;
  std::unique_ptr<State> state;
};

template<typename Scheduler, typename MessageHandler, typename State>
void send(Actor<Scheduler, MessageHandler, State>& actor,
          std::unique_ptr<ActorMessageBase> msg) {
  actor.process(std::move(msg));
}

}  // namespace arangodb::pregel::actor
