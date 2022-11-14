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
#include <variant>

#include "Actor/MPSCQueue.h"
#include "gtest/gtest.h"
#include "Actor/Actor.h"

#include "fmt/core.h"

#include "Basics/ThreadGuard.h"

using namespace arangodb;
using namespace arangodb::pregel::actor;
using namespace arangodb::pregel::mpscqueue;

// This scheduler just runs any function synchronously as soon as it comes in.
struct Scheduler {
  auto operator()(auto fn) { fn(); }
};

struct State {
  virtual ~State() = default;
  virtual auto name() const -> std::string = 0;
};
struct InitialState : State {
  ~InitialState() = default;
  auto name() const -> std::string override { return "initial"; };
};
struct LoadingState : State {
  ~LoadingState() = default;
  auto name() const -> std::string override { return "loading"; };
};

struct InitStart {};
struct InitDone {};
struct MessagePayload : std::variant<InitStart, InitDone> {
  using std::variant<InitStart, InitDone>::variant;
};
struct Message : public MessagePayload, public MPSCQueue<Message>::Node {
  using MessagePayload::MessagePayload;
};

struct InitialHandler {
  InitialHandler() = default;
  InitialHandler(std::unique_ptr<State> state) : state{std::move(state)} {}
  std::unique_ptr<State> state;

  auto operator()(InitStart& msg) -> std::unique_ptr<State> {
    fmt::print("got start message");
    return std::move(state);
  }
  auto operator()(InitDone& msg) -> std::unique_ptr<State> {
    fmt::print("got done message");
    return std::make_unique<LoadingState>();
  }
  auto operator()(auto&& msg) -> std::unique_ptr<State> {
    fmt::print("got any message");
    return std::move(state);
  }
};

using InitialActor = Actor<Scheduler, InitialHandler, State, Message>;

TEST(Actor, acts_intially) {
  auto scheduler = Scheduler{};
  auto actor = InitialActor(scheduler, std::make_unique<InitialState>());

  ASSERT_EQ(actor.state->name(), "initial");
  send(actor, std::make_unique<Message>(InitDone{}));
  ASSERT_EQ(actor.state->name(), "loading");
}
