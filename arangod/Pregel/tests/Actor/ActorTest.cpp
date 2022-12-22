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

#include "Pregel/Actor/Message.h"
#include "gtest/gtest.h"
#include "Pregel/Actor/Actor.h"
#include "Pregel/Actor/ActorPID.h"
#include "Pregel/Actor/Actors/TrivialActor.h"

#include "fmt/core.h"
#include <Inspection/VPackWithErrorT.h>

using namespace arangodb::pregel::actor;
using namespace arangodb::pregel::actor::test;

struct MockScheduler {
  auto operator()(auto fn) { fn(); }
};

TEST(ActorTest, has_a_type_name) {
  auto scheduler = std::make_shared<MockScheduler>();
  auto actor = Actor<MockScheduler, TrivialActor>(
      ActorPID{}, scheduler, nullptr, std::make_unique<TrivialState>());
  ASSERT_EQ(actor.typeName(), "TrivialActor");
}

TEST(ActorTest, formats_actor) {
  auto scheduler = std::make_shared<MockScheduler>();
  auto actor = Actor<MockScheduler, TrivialActor>(
      ActorPID{.server = "A", .id = {1}}, scheduler, nullptr,
      std::make_unique<TrivialState>());
  ASSERT_EQ(
      fmt::format("{}", actor),
      R"({"pid":{"server":"A","id":1,"databaseName":""},"state":{"state":"","called":0},"batchsize":16})");
}

TEST(ActorTest, changes_its_state_after_processing_a_message) {
  auto scheduler = std::make_shared<MockScheduler>();
  auto actor = Actor<MockScheduler, TrivialActor>(
      ActorPID{.server = "A", .id = {1}}, scheduler, nullptr,
      std::make_unique<TrivialState>());
  ASSERT_EQ(*actor.state, (TrivialState{.state = "", .called = 0}));
  auto message = std::make_unique<MessagePayload<TrivialMessage>>(
      TrivialMessage1{"Hello"});
  actor.process(ActorPID{.server = "A", .id = {5}}, std::move(message));
  ASSERT_EQ(*actor.state, (TrivialState{.state = "Hello", .called = 1}));
}

// TODO error handling when actor receives unknown message

TEST(ActorTest, changes_its_state_after_processing_a_velocypack_message) {
  auto scheduler = std::make_shared<MockScheduler>();
  auto actor = Actor<MockScheduler, TrivialActor>(
      ActorPID{.server = "A", .id = {1}}, scheduler, nullptr,
      std::make_unique<TrivialState>());
  ASSERT_EQ(*actor.state, (TrivialState{.state = "", .called = 0}));
  auto message = TrivialMessage{TrivialMessage1{"Hello"}};
  actor.process(ActorPID{.server = "A", .id = {5}},
                arangodb::inspection::serializeWithErrorT(message).get());
  ASSERT_EQ(*actor.state, (TrivialState{.state = "Hello", .called = 1}));
}

// TODO error handling when actor received unknown velocypack message
