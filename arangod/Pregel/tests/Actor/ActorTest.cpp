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
  std::string state;
  std::size_t called;
};

struct TrivialActorMessage : public MPSCQueue<TrivialActorMessage>::Node {
  TrivialActorMessage(std::string value) : store(std::move(value)) {}
  std::string store;
};

struct TrivialHandler {
  auto operator()(TrivialState state, std::unique_ptr<TrivialActorMessage> msg)
      -> TrivialState {
    state.called++;
    state.state += msg->store;
    return state;
  }
};

using MyActor = Actor<TrivialScheduler, TrivialHandler, TrivialState, TrivialActorMessage>;

TEST(Actor, processes_message) {
  auto scheduler = TrivialScheduler{};

  auto actor = MyActor(scheduler, {.state = "Hello"});

  send(actor, std::make_unique<TrivialActorMessage>("hello"));
  send(actor, std::make_unique<TrivialActorMessage>("world"));
  send(actor, std::make_unique<TrivialActorMessage>("!"));

  ASSERT_EQ(actor.state.called, 3u);
  ASSERT_EQ(actor.state.state, "Hellohelloworld!");
}

struct NonTrivialScheduler {
  NonTrivialScheduler() {}

  auto operator()(auto fn) { threads.emplace(fn); }

  ThreadGuard threads;
};

struct NonTrivialState {
  std::string state;
  std::size_t called;
};

struct NonTrivialActorMessage : public MPSCQueue<NonTrivialActorMessage>::Node {
  NonTrivialActorMessage(std::string value) : store(std::move(value)) {}
  std::string store;
};

struct NonTrivialHandler {
  auto operator()(NonTrivialState state, std::unique_ptr<NonTrivialActorMessage> msg)
      -> NonTrivialState {
    state.called++;
    state.state += msg->store;
    return state;
  }
};

using MyActor2 =
    Actor<NonTrivialScheduler, NonTrivialHandler, NonTrivialState, NonTrivialActorMessage>;

TEST(Actor, trivial_thread_scheduler) {
   auto scheduler = NonTrivialScheduler();
   auto actor = MyActor2(scheduler, {.state = "Hello"});

   for (std::size_t i = 0; i < 100; i++) {
     send(actor, std::make_unique<NonTrivialActorMessage>("hello"));
     send(actor, std::make_unique<NonTrivialActorMessage>("world"));
     send(actor, std::make_unique<NonTrivialActorMessage>("!"));
   }
   // joinen.
   scheduler.threads.joinAll();
   fmt::print("I got called {} times, accumulated {}\n", actor.state.called,
              actor.state.state);
}
