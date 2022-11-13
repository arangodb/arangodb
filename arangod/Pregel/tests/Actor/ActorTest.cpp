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

struct State {
  std::string state;
  std::size_t called;
};

struct ActorMessage : public MPSCQueue<ActorMessage>::Node {
  ActorMessage(std::string value) : store(std::move(value)) {}
  std::string store;
};

struct Handler {
  auto operator()(State state, std::unique_ptr<ActorMessage> msg)
      -> State {
    state.called++;
    state.state += msg->store;
    return state;
  }
};

using MyActor = Actor<TrivialScheduler, Handler, State, ActorMessage>;

TEST(Actor, processes_message) {
  auto scheduler = TrivialScheduler{};

  auto actor = MyActor(scheduler, {.state = "Hello"});

  send(actor, std::make_unique<ActorMessage>("hello"));
  send(actor, std::make_unique<ActorMessage>("world"));
  send(actor, std::make_unique<ActorMessage>("!"));

  ASSERT_EQ(actor.state.called, 3u);
  ASSERT_EQ(actor.state.state, "Hellohelloworld!");
}

struct SlightlyNonTrivialScheduler {
  SlightlyNonTrivialScheduler() {}

  auto operator()(auto fn) { threads.emplace(fn); }

  ThreadGuard threads;
};

struct ActorMessageA : public MPSCQueue<ActorMessageA>::Node {
  ActorMessageA(std::string value) : store(std::move(value)) {}
  std::string store;
};

using MyActor2 =
    Actor<SlightlyNonTrivialScheduler, Handler, State, ActorMessageA>;
struct State {
  std::string state;
  std::size_t called;
};

struct ActorMessage : public MPSCQueue<ActorMessage>::Node {
  ActorMessage(std::string value) : store(std::move(value)) {}
  std::string store;
};

struct Handler {
  auto operator()(State state, std::unique_ptr<ActorMessage> msg)
      -> State {
    state.called++;
    state.state += msg->store;
    return state;
  }
};



TEST(Actor, trivial_thread_scheduler) {
   auto scheduler = SlightlyNonTrivialScheduler();
   auto actor = MyActor2(scheduler, {.state = "Hello"});

//   for (std::size_t i = 0; i < 100; i++) {
//     send(actor, std::make_unique<Message>("hello"));
//     send(actor, std::make_unique<Message>("world"));
//     send(actor, std::make_unique<Message>("!"));
//   }
//   // joinen.
//   fmt::print("I got called {} times, accumulated {}\n", actor.state.called,
//              actor.state.state);

//   scheduler.threads.joinAll();
}
