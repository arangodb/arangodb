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

struct ConductorInitialState {
  // workerApi
};

struct InitialHandler {
  auto operator()(std::unique_ptr<ConductorInitialState> state,
                  std::unique_ptr<InitStart> msg)
      -> std::unique_ptr<TestConductorState> {

  }
  auto operator()(std::unique_ptr<ConductorInitialState> state,
                  std::unique_ptr<InitDone> msg)
      -> std::unique_ptr<TestConductorState> {

  }
  auto operator()(std::unique_ptr<ConductorInitialState> state,
                  std::unique_ptr<???> msg)
      -> std::unique_ptr<TestConductorState> {

  }
};

using ConductorInitialActor = Actor<ConductorScheduler, InitialHandler, ConductorInitialState, TestConductorMessage>;

TEST(IntialActor, acts_intially) {

}
