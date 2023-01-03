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

#include "Pregel/Actor/ActorPID.h"
#include "Pregel/Actor/Actors/SpawnActor.h"
#include "Pregel/Actor/Runtime.h"
#include "Pregel/Actor/Actors/TrivialActor.h"
#include "Pregel/Actor/Actors/PingPongActors.h"

#include "fmt/format.h"
#include "velocypack/SharedSlice.h"

using namespace arangodb::pregel::actor;
using namespace arangodb::pregel::actor::test;

struct MockScheduler {
  auto operator()(auto fn) { fn(); }
};
// struct NonTrivialScheduler {
//   NonTrivialScheduler() {}
//   auto operator()(auto fn) { threads.emplace(fn); }
//   ThreadGuard threads;
// };

struct EmptyExternalDispatcher {
  auto operator()(ActorPID sender, ActorPID receiver,
                  arangodb::velocypack::SharedSlice msg) -> void {}
};

using MockRuntime = Runtime<MockScheduler, EmptyExternalDispatcher>;

TEST(RuntimeTest, formats_runtime_and_actor_state) {
  auto scheduler = std::make_shared<MockScheduler>();
  auto dispatcher = std::make_shared<EmptyExternalDispatcher>();
  auto runtime = std::make_shared<MockRuntime>(
      ServerID{"PRMR-1234"}, "RuntimeTest", scheduler, dispatcher);
  auto actorID = runtime->spawn<TrivialActor>(TrivialState{.state = "foo"},
                                              TrivialMessage0{});
  ASSERT_EQ(
      fmt::format("{}", *runtime),
      R"({"myServerID":"PRMR-1234","runtimeID":"RuntimeTest","uniqueActorIDCounter":1,"actors":[{"id":0,"type":"TrivialActor"}]})");
  auto actor = runtime->getActorStateByID<TrivialActor>(actorID).value();
  ASSERT_EQ(fmt::format("{}", actor), R"({"state":"foo","called":1})");
}

TEST(RuntimeTest, spawns_actor) {
  auto scheduler = std::make_shared<MockScheduler>();
  auto dispatcher = std::make_shared<EmptyExternalDispatcher>();
  auto runtime = std::make_shared<MockRuntime>("PRMR-1234", "RuntimeTest",
                                               scheduler, dispatcher);

  auto actor = runtime->spawn<TrivialActor>(TrivialState{.state = "foo"},
                                            TrivialMessage0());

  auto state = runtime->getActorStateByID<TrivialActor>(actor);
  ASSERT_EQ(state, (TrivialState{.state = "foo", .called = 1}));
}

TEST(RuntimeTest, sends_initial_message_when_spawning_actor) {
  auto scheduler = std::make_shared<MockScheduler>();
  auto dispatcher = std::make_shared<EmptyExternalDispatcher>();
  auto runtime = std::make_shared<MockRuntime>("PRMR-1234", "RuntimeTest",
                                               scheduler, dispatcher);

  auto actor = runtime->spawn<TrivialActor>(TrivialState{.state = "foo"},
                                            TrivialMessage1("bar"));

  auto state = runtime->getActorStateByID<TrivialActor>(actor);
  ASSERT_EQ(state, (TrivialState{.state = "foobar", .called = 1}));
}

TEST(RuntimeTest, gives_all_existing_actor_ids) {
  auto scheduler = std::make_shared<MockScheduler>();
  auto dispatcher = std::make_shared<EmptyExternalDispatcher>();
  auto runtime = std::make_shared<MockRuntime>("PRMR-1234", "RuntimeTest",
                                               scheduler, dispatcher);

  ASSERT_TRUE(runtime->getActorIDs().empty());

  auto actor_foo = runtime->spawn<TrivialActor>(TrivialState{.state = "foo"},
                                                TrivialMessage0());
  auto actor_bar = runtime->spawn<TrivialActor>(TrivialState{.state = "bar"},
                                                TrivialMessage0());

  auto allActorIDs = runtime->getActorIDs();
  ASSERT_EQ(allActorIDs.size(), 2);
  ASSERT_EQ(
      (std::unordered_set<ActorID>(allActorIDs.begin(), allActorIDs.end())),
      (std::unordered_set<ActorID>{actor_foo, actor_bar}));
}

TEST(RuntimeTest, sends_message_to_an_actor) {
  auto scheduler = std::make_shared<MockScheduler>();
  auto dispatcher = std::make_shared<EmptyExternalDispatcher>();
  auto runtime = std::make_shared<MockRuntime>("PRMR-1234", "RuntimeTest",
                                               scheduler, dispatcher);
  auto actor = runtime->spawn<TrivialActor>(TrivialState{.state = "foo"},
                                            TrivialMessage0{});

  runtime->dispatch(
      ActorPID{.server = "Foo", .id = actor},
      ActorPID{.server = "PRMR-1234", .id = actor},
      std::make_unique<MessagePayload<MessageOrError<TrivialActor::Message>>>(
          TrivialMessage1("baz")));

  auto state = runtime->getActorStateByID<TrivialActor>(actor);
  ASSERT_EQ(state, (TrivialState{.state = "foobaz", .called = 2}));
}

// struct SomeMessage {};
// struct SomeMessages : std::variant<SomeMessage> {
//   using std::variant<SomeMessage>::variant;
// };
// TEST(RuntimeTest, sends_message_with_wrong_type_to_an_actor) {
//   auto scheduler = std::make_shared<MockScheduler>();
//   auto dispatcher = std::make_shared<EmptyExternalDispatcher>();
//   auto runtime = std::make_shared<MockRuntime>("PRMR-1234", "RuntimeTest",
//                                                scheduler, dispatcher);
//   auto actor = runtime->spawn<TrivialActor>(TrivialState{.state = "foo"},
//                                             TrivialMessage0{});

//   runtime->dispatch(
//       ActorPID{.server = "Foo", .id = actor},
//       ActorPID{.server = "PRMR-1234", .id = actor},
//       std::make_unique<MessagePayload<MessageOrError<SomeMessages>>>(
//           SomeMessage()));

//   // auto state = runtime.getActorStateByID<TrivialActor>(actor);
//   // ASSERT_EQ(state, (TrivialState{.state = "foobaz", .called = 2}));
// }

TEST(RuntimeTest, ping_pong_game) {
  auto serverID = ServerID{"PRMR-1234"};
  auto scheduler = std::make_shared<MockScheduler>();
  auto dispatcher = std::make_shared<EmptyExternalDispatcher>();
  auto runtime = std::make_shared<MockRuntime>(serverID, "RuntimeTest",
                                               scheduler, dispatcher);

  auto pong_actor = runtime->spawn<pong_actor::Actor>(pong_actor::State{},
                                                      pong_actor::Start{});
  auto ping_actor = runtime->spawn<ping_actor::Actor>(
      ping_actor::State{},
      ping_actor::Start{.pongActor =
                            ActorPID{.server = serverID, .id = pong_actor}});

  auto ping_actor_state =
      runtime->getActorStateByID<ping_actor::Actor>(ping_actor);
  ASSERT_EQ(ping_actor_state,
            (ping_actor::State{.called = 2, .message = "hello world"}));
  auto pong_actor_state =
      runtime->getActorStateByID<pong_actor::Actor>(pong_actor);
  ASSERT_EQ(pong_actor_state, (pong_actor::State{.called = 2}));
}

TEST(RuntimeTest, spawn_game) {
  auto serverID = ServerID{"PRMR-1234"};
  auto scheduler = std::make_shared<MockScheduler>();
  auto dispatcher = std::make_shared<EmptyExternalDispatcher>();
  auto runtime = std::make_shared<MockRuntime>(serverID, "RuntimeTest",
                                               scheduler, dispatcher);

  auto spawn_actor =
      runtime->spawn<SpawnActor>(SpawnState{}, SpawnStartMessage{});

  runtime->dispatch(
      ActorPID{.server = serverID, .id = spawn_actor},
      ActorPID{.server = serverID, .id = spawn_actor},
      std::make_unique<MessagePayload<MessageOrError<SpawnActor::Message>>>(
          SpawnMessage("baz")));

  auto allActors = runtime->getActorIDs();
  ASSERT_EQ(allActors.size(), 2);

  auto spawn_actor_state = runtime->getActorStateByID<SpawnActor>(spawn_actor);
  ASSERT_EQ(spawn_actor_state, (SpawnState{.called = 2, .state = "baz"}));
}
