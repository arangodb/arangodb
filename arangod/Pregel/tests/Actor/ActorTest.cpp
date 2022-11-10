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

#include "gtest/gtest.h"
#include "Actor/Actor.h"

#include "fmt/core.h"

using namespace arangodb::pregel::actor;

// This scheduler just runs any function synchronously as soon as it comes in.
struct TrivialScheduler {
  auto operator()(auto fn) { fn(); }
};

struct State {
  std::string state;
  std::size_t called;
};

struct Message {
  std::string store;
};

struct Handler {
  auto operator()(State state, Message msg) -> State {
    fmt::print("message handler called\n");
    state.called++;
    state.state += msg.store;
    return state;
  }
};

using MyActor = Actor<TrivialScheduler, Handler, State, Message>;

TEST(Actor, processes_message) {
  auto scheduler = TrivialScheduler{};

  auto actor = MyActor(scheduler, {.state = "Hello"});

  send(actor, {.store = "hello"});
  send(actor, {.store = "world"});
  send(actor, {.store = "!"});

  fmt::print("I got called {} times, accumulated {}\n", actor.state.called,
             actor.state.state);
}

struct SlightlyNonTrivialScheduler {
  SlightlyNonTrivialScheduler() {}

  auto operator()(auto fn) { threads.emplace_back(fn); }

  // TODO: use threadGuard?
  std::vector<std::thread> threads;
};

using MyActor2 = Actor<SlightlyNonTrivialScheduler, Handler, State, Message>;
TEST(Actor, trivial_thread_scheduler) {
  auto scheduler = SlightlyNonTrivialScheduler();

  auto actor = MyActor2(scheduler, {.state = "Hello"});

  for (std::size_t i = 0; i < 100; i++) {
    send(actor, {.store = "hello"});
    send(actor, {.store = "world"});
    send(actor, {.store = "!"});
  }
  // joinen.
  fmt::print("I got called {} times, accumulated {}\n", actor.state.called,
             actor.state.state);
}
