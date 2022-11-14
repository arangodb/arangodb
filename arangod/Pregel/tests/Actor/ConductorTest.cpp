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

#include "gtest/gtest.h"
#include "Actor/Actor.h"

#include "fmt/core.h"

#include "Basics/ThreadGuard.h"

using namespace arangodb;
using namespace arangodb::pregel::actor;
using namespace arangodb::pregel::mpscqueue;

// This scheduler just runs any function synchronously as soon as it comes in.
struct ConductorScheduler {
  auto operator()(auto fn) { fn(); }
};

struct TestConductorState;
struct TestConductorMessage;

struct TestConductorState {
  virtual auto name() const -> std::string = 0;
  virtual auto work(std::unique_ptr<TestConductorMessage> msg)
      -> std::unique_ptr<TestConductorState> = 0;
  virtual ~TestConductorState() = default;
};

struct TestConductorLoading : TestConductorState {
  auto name() const -> std::string override { return "loading"; };
  auto work(std::unique_ptr<TestConductorMessage> msg)
      -> std::unique_ptr<TestConductorState> override {
    return nullptr;
  }
  ~TestConductorLoading() = default;
};

struct TestConductorInitial : TestConductorState {
  auto name() const -> std::string override { return "initial"; };
  auto work(std::unique_ptr<TestConductorMessage> msg)
      -> std::unique_ptr<TestConductorState> override {
    return std::make_unique<TestConductorLoading>();
  }
  ~TestConductorInitial() = default;
};

struct InitStart {};
struct InitDone {};
struct MessagePayload : std::variant<InitStart, InitDone> {};
struct TestConductorMessage : public MessagePayload,
                              public MPSCQueue<TestConductorMessage>::Node {};

struct TestConductorHandler {
  auto operator()(std::unique_ptr<TestConductorState> state,
                  std::unique_ptr<TestConductorMessage> msg)
      -> std::unique_ptr<TestConductorState> {
    return state->work(std::move(msg));
  }
};

using TestConductor = Actor<ConductorScheduler, TestConductorHandler,
                            TestConductorState, TestConductorMessage>;

TEST(Actor, acts_like_a_conductor) {
  auto scheduler = ConductorScheduler();
  auto conductor =
      TestConductor(scheduler, std::make_unique<TestConductorInitial>());

  ASSERT_EQ(conductor.state->name(), "initial");

  send(conductor, std::make_unique<TestConductorMessage>());

  ASSERT_EQ(conductor.state->name(), "loading");

}
