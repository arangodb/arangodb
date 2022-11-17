////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#include <chrono>
#include <thread>
#include <string>
#include <memory>

#include "gtest/gtest.h"
#include "Actor/Actor.h"

#include "fmt/core.h"

#include "Basics/ThreadGuard.h"

using namespace arangodb;
using namespace arangodb::pregel::actor;
using namespace arangodb::pregel::mpscqueue;

// This scheduler just runs any function synchronously as soon as it comes in.
struct TrivialScheduler {
  auto operator()(auto fn) { fn(); }
};

struct TrivialState {
  TrivialState(std::string state) : state(std::move(state)) {}
  std::string state;
  std::size_t called{};
};

struct SpecificMessage {
  SpecificMessage(std::string value) : store(std::move(value)) {}
  std::string store;
};

struct ActorMessage : public std::variant<SpecificMessage>,
                      public MPSCQueue<ActorMessage>::Node {
  using std::variant<SpecificMessage>::variant;
};

struct TrivialHandler {
  TrivialHandler(std::unique_ptr<TrivialState> state)
      : state{std::move(state)} {};
  std::unique_ptr<TrivialState> state;
  auto operator()(SpecificMessage msg) -> std::unique_ptr<TrivialState> {
    state->called++;
    state->state += msg.store;
    return std::move(state);
  }
};

using MyActor =
    Actor<TrivialScheduler, TrivialHandler, TrivialState, ActorMessage>;

TEST(Actor, processes_message) {
  auto scheduler = TrivialScheduler{};

  auto actor = MyActor(scheduler, std::make_unique<TrivialState>("Hello"));

  send(actor, std::make_unique<ActorMessage>(SpecificMessage{"hello"}));
  send(actor, std::make_unique<ActorMessage>(SpecificMessage{"world"}));
  send(actor, std::make_unique<ActorMessage>(SpecificMessage{"!"}));

  ASSERT_EQ(actor.state->called, 3u);
  ASSERT_EQ(actor.state->state, "Hellohelloworld!");
}

struct NonTrivialScheduler {
  NonTrivialScheduler() {}

  auto operator()(auto fn) { threads.emplace(fn); }

  ThreadGuard threads;
};

struct NonTrivialState {
  NonTrivialState(std::string state) : state(std::move(state)) {}
  std::string state;
  std::size_t called{};
};

struct NonTrivialHandler {
  NonTrivialHandler(std::unique_ptr<NonTrivialState> state)
      : state{std::move(state)} {};
  std::unique_ptr<NonTrivialState> state;
  auto operator()(SpecificMessage msg) -> std::unique_ptr<NonTrivialState> {
    state->called++;
    state->state += msg.store;
    return std::move(state);
  }
};

using MyActor2 = Actor<NonTrivialScheduler, NonTrivialHandler, NonTrivialState,
                       ActorMessage>;

TEST(Actor, trivial_thread_scheduler) {
  auto scheduler = NonTrivialScheduler();
  auto actor = MyActor2(scheduler, std::make_unique<NonTrivialState>("Hello"));

  for (std::size_t i = 0; i < 100; i++) {
    send(actor, std::make_unique<ActorMessage>(SpecificMessage{"hello"}));
    send(actor, std::make_unique<ActorMessage>(SpecificMessage{"world"}));
    send(actor, std::make_unique<ActorMessage>(SpecificMessage{"!"}));
  }
  // joinen.
  scheduler.threads.joinAll();
  ASSERT_EQ(actor.state->called, 300);
  ASSERT_EQ(actor.state->state.length(), 5 + 100 * (5 + 5 + 1));
}
