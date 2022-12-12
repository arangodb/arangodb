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
/// @author Julia Volmer
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include <unordered_set>

#include "Actor/ActorPID.h"
#include "Actor/Runtime.h"

#include "TrivialActor.h"

using namespace arangodb::pregel::actor;
using namespace arangodb::pregel::actor::test;

struct MockScheduler {
  auto operator()(auto fn) { fn(); }
};

struct MockSendingMechanism {};

TEST(RuntimeTest, spawns_actor) {
  auto scheduler = std::make_shared<MockScheduler>();
  auto sendingMechanism = std::make_shared<MockSendingMechanism>();
  Runtime runtime("PRMR-1234", "RuntimeTest", scheduler, sendingMechanism);

  auto actor = runtime.spawn<TrivialState, TrivialMessage, TrivialHandler>(
      TrivialState{.state = "foo"}, TrivialMessage0());

  auto state =
      runtime.getActorStateByID<TrivialState, TrivialMessage, TrivialHandler>(
          actor);
  ASSERT_EQ(state, (TrivialState{.state = "foo", .called = 1}));
}

TEST(RuntimeTest, sends_initial_message_when_spawning_actor) {
  auto scheduler = std::make_shared<MockScheduler>();
  auto sendingMechanism = std::make_shared<MockSendingMechanism>();
  Runtime runtime("PRMR-1234", "RuntimeTest", scheduler, sendingMechanism);

  auto actor = runtime.spawn<TrivialState, TrivialMessage, TrivialHandler>(
      TrivialState{.state = "foo"}, TrivialMessage1("bar"));

  auto state =
      runtime.getActorStateByID<TrivialState, TrivialMessage, TrivialHandler>(
          actor);
  ASSERT_EQ(state, (TrivialState{.state = "foobar", .called = 1}));
}

TEST(RuntimeTest, gives_all_existing_actor_ids) {
  auto scheduler = std::make_shared<MockScheduler>();
  auto sendingMechanism = std::make_shared<MockSendingMechanism>();
  Runtime runtime("PRMR-1234", "RuntimeTest", scheduler, sendingMechanism);

  ASSERT_TRUE(runtime.getActorIDs().empty());

  auto actor_foo = runtime.spawn<TrivialState, TrivialMessage, TrivialHandler>(
      TrivialState{.state = "foo"}, TrivialMessage0());
  auto actor_bar = runtime.spawn<TrivialState, TrivialMessage, TrivialHandler>(
      TrivialState{.state = "bar"}, TrivialMessage0());

  auto allActorIDs = runtime.getActorIDs();
  ASSERT_EQ(allActorIDs.size(), 2);
  ASSERT_EQ(
      (std::unordered_set<ActorID>(allActorIDs.begin(), allActorIDs.end())),
      (std::unordered_set<ActorID>{actor_foo, actor_bar}));
}

TEST(RuntimeTest, sends_message_to_an_actor) {
  auto scheduler = std::make_shared<MockScheduler>();
  auto sendingMechanism = std::make_shared<MockSendingMechanism>();
  Runtime runtime("PRMR-1234", "RuntimeTest", scheduler, sendingMechanism);
  auto actor = runtime.spawn<TrivialState, TrivialMessage, TrivialHandler>(
      TrivialState{.state = "foo"}, TrivialMessage0{});

  runtime.dispatch(std::make_unique<Message>(
      ActorPID{.id = actor, .server = "Foo"},
      ActorPID{.id = actor, .server = "PRMR-1234"},
      std::make_unique<MessagePayload<TrivialMessage>>(
          TrivialMessage1("baz"))));

  auto state =
      runtime.getActorStateByID<TrivialState, TrivialMessage, TrivialHandler>(
          actor);
  ASSERT_EQ(state, (TrivialState{.state = "foobaz", .called = 2}));
}

// TEST(RuntimeTest, sends_message_with_wrong_type_to_an_actor) {
//   // TODO what happens then? currently aborts
// }
