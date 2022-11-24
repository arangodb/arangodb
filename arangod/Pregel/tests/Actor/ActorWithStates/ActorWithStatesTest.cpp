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

#include <algorithm>
#include <chrono>
#include <thread>
#include <string>
#include <memory>
#include <variant>

#include "Actor/MPSCQueue.h"
#include "gtest/gtest.h"
#include "Actor/Actor.h"
#include "ConductorActor.h"
#include "InitialActor.h"

#include "fmt/core.h"

#include "Basics/ThreadGuard.h"
#include "tests/Actor/ActorWithStates/Message.h"

using namespace arangodb;
using namespace arangodb::pregel::actor;
using namespace arangodb::pregel::mpscqueue;

struct LoadingState {
  ~LoadingState() = default;
  auto name() const -> std::string { return "loading"; };
};

struct LoadingHandler {
  LoadingHandler() = default;
  LoadingHandler(std::unique_ptr<LoadingState> state)
      : state{std::move(state)} {}
  std::unique_ptr<LoadingState> state;
};

TEST(Actor, acts_intially) {
  auto scheduler = Scheduler{};
  auto conductorActor =
      ConductorActor(scheduler, std::make_unique<Conductor>());

  conductorActor.process(std::make_unique<Message>(nullptr, InitConductor{}));
  // ASSERT_EQ(actor.state->name(), "initial");
  // send(actor, std::make_unique<Message>(InitDone{}));
  // ASSERT_EQ(actor.state->name(), "loading");
}
