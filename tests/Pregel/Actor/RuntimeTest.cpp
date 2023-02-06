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
#include "Actors/FinishingActor.h"
#include "Actors/SpawnActor.h"
#include "Actors/TrivialActor.h"
#include "Actors/PingPongActors.h"

#include "fmt/format.h"
#include "velocypack/SharedSlice.h"
#include "VelocypackUtils/VelocyPackStringLiteral.h"

using namespace arangodb::pregel::actor;
using namespace arangodb::pregel::actor::test;

struct MockScheduler {
  auto operator()(auto fn) { fn(); }
};

struct EmptyExternalDispatcher {
  auto operator()(ActorPID sender, ActorPID receiver,
                  arangodb::velocypack::SharedSlice msg) -> void {}
};

using MockRuntime = Runtime<MockScheduler, EmptyExternalDispatcher>;

TEST(ActorRuntimeTest, formats_runtime_and_actor_state) {
  auto scheduler = std::make_shared<MockScheduler>();
  auto dispatcher = std::make_shared<EmptyExternalDispatcher>();
  auto runtime = std::make_shared<MockRuntime>(
      ServerID{"PRMR-1234"}, "RuntimeTest", scheduler, dispatcher);
  auto actorID = runtime->spawn<pong_actor::Actor>(pong_actor::PongState{},
                                                   pong_actor::Start{});
  ASSERT_EQ(
      fmt::format("{}", *runtime),
      R"({"myServerID":"PRMR-1234","runtimeID":"RuntimeTest","uniqueActorIDCounter":1,"actors":[{"id":0,"type":"PongActor"}]})");
  auto actor = runtime->getActorStateByID<pong_actor::Actor>(actorID).value();
  ASSERT_EQ(fmt::format("{}", actor), R"({"called":1})");
}

TEST(ActorRuntimeTest, serializes_an_actor_including_its_actor_state) {
  auto scheduler = std::make_shared<MockScheduler>();
  auto dispatcher = std::make_shared<EmptyExternalDispatcher>();
  auto runtime = std::make_shared<MockRuntime>(
      ServerID{"PRMR-1234"}, "RuntimeTest", scheduler, dispatcher);
  auto actor = runtime->spawn<TrivialActor>(TrivialState{.state = "foo"},
                                            TrivialStart());

  using namespace arangodb::velocypack;
  auto expected =
      R"({"pid":{"server":"PRMR-1234","id":0},"state":{"state":"foo","called":1},"batchsize":16})"_vpack;
  ASSERT_EQ(runtime->getSerializedActorByID(actor)->toJson(),
            expected.toJson());
}

TEST(ActorRuntimeTest, spawns_actor) {
  auto scheduler = std::make_shared<MockScheduler>();
  auto dispatcher = std::make_shared<EmptyExternalDispatcher>();
  auto runtime = std::make_shared<MockRuntime>("PRMR-1234", "RuntimeTest",
                                               scheduler, dispatcher);

  auto actor = runtime->spawn<TrivialActor>(TrivialState{.state = "foo"},
                                            TrivialStart());

  auto state = runtime->getActorStateByID<TrivialActor>(actor);
  ASSERT_EQ(state, (TrivialState{.state = "foo", .called = 1}));
}

TEST(ActorRuntimeTest, sends_initial_message_when_spawning_actor) {
  auto scheduler = std::make_shared<MockScheduler>();
  auto dispatcher = std::make_shared<EmptyExternalDispatcher>();
  auto runtime = std::make_shared<MockRuntime>("PRMR-1234", "RuntimeTest",
                                               scheduler, dispatcher);

  auto actor = runtime->spawn<TrivialActor>(TrivialState{.state = "foo"},
                                            TrivialMessage("bar"));

  auto state = runtime->getActorStateByID<TrivialActor>(actor);
  ASSERT_EQ(state, (TrivialState{.state = "foobar", .called = 1}));
}

TEST(ActorRuntimeTest, gives_all_existing_actor_ids) {
  auto scheduler = std::make_shared<MockScheduler>();
  auto dispatcher = std::make_shared<EmptyExternalDispatcher>();
  auto runtime = std::make_shared<MockRuntime>("PRMR-1234", "RuntimeTest",
                                               scheduler, dispatcher);

  ASSERT_TRUE(runtime->getActorIDs().empty());

  auto actor_foo = runtime->spawn<TrivialActor>(TrivialState{.state = "foo"},
                                                TrivialStart());
  auto actor_bar = runtime->spawn<TrivialActor>(TrivialState{.state = "bar"},
                                                TrivialStart());

  auto allActorIDs = runtime->getActorIDs();
  ASSERT_EQ(allActorIDs.size(), 2);
  ASSERT_EQ(
      (std::unordered_set<ActorID>(allActorIDs.begin(), allActorIDs.end())),
      (std::unordered_set<ActorID>{actor_foo, actor_bar}));
}

TEST(ActorRuntimeTest, sends_message_to_an_actor) {
  auto scheduler = std::make_shared<MockScheduler>();
  auto dispatcher = std::make_shared<EmptyExternalDispatcher>();
  auto runtime = std::make_shared<MockRuntime>("PRMR-1234", "RuntimeTest",
                                               scheduler, dispatcher);
  auto actor = runtime->spawn<TrivialActor>(TrivialState{.state = "foo"},
                                            TrivialStart{});

  runtime->dispatch(ActorPID{.server = "PRMR-1234", .id = actor},
                    ActorPID{.server = "PRMR-1234", .id = actor},
                    TrivialActor::Message{TrivialMessage("baz")});

  auto state = runtime->getActorStateByID<TrivialActor>(actor);
  ASSERT_EQ(state, (TrivialState{.state = "foobaz", .called = 2}));
}

struct SomeMessage {};
template<typename Inspector>
auto inspect(Inspector& f, SomeMessage& x) {
  return f.object(x).fields();
}
struct SomeMessages : std::variant<SomeMessage> {
  using std::variant<SomeMessage>::variant;
};
template<typename Inspector>
auto inspect(Inspector& f, SomeMessages& x) {
  return f.variant(x).unqualified().alternatives(
      arangodb::inspection::type<SomeMessage>("someMessage"));
}
TEST(ActorRuntimeTest,
     actor_receiving_wrong_message_type_sends_back_unknown_error_message) {
  auto scheduler = std::make_shared<MockScheduler>();
  auto dispatcher = std::make_shared<EmptyExternalDispatcher>();
  auto runtime = std::make_shared<MockRuntime>("PRMR-1234", "RuntimeTest",
                                               scheduler, dispatcher);
  auto actor_id = runtime->spawn<TrivialActor>(TrivialState{.state = "foo"},
                                               TrivialStart{});
  auto actor = ActorPID{.server = "PRMR-1234", .id = actor_id};

  runtime->dispatch(actor, actor, SomeMessages{SomeMessage{}});

  ASSERT_EQ(
      runtime->getActorStateByID<TrivialActor>(actor_id),
      (TrivialState{.state = fmt::format("sent unknown message to {}", actor),
                    .called = 2}));
}

TEST(
    ActorRuntimeTest,
    actor_receives_actor_not_found_message_after_trying_to_send_message_to_non_existent_actor) {
  auto scheduler = std::make_shared<MockScheduler>();
  auto dispatcher = std::make_shared<EmptyExternalDispatcher>();
  auto runtime = std::make_shared<MockRuntime>("PRMR-1234", "RuntimeTest",
                                               scheduler, dispatcher);
  auto actor_id = runtime->spawn<TrivialActor>(TrivialState{.state = "foo"},
                                               TrivialStart{});
  auto actor = ActorPID{.server = "PRMR-1234", .id = actor_id};

  auto unknown_actor = ActorPID{.server = "PRMR-1234", .id = {999}};
  runtime->dispatch(actor, unknown_actor,
                    TrivialActor::Message{TrivialMessage{"baz"}});

  ASSERT_EQ(runtime->getActorStateByID<TrivialActor>(actor_id),
            (TrivialState{.state = fmt::format("receiving actor {} not found",
                                               unknown_actor),
                          .called = 2}));
}

TEST(ActorRuntimeTest, ping_pong_game) {
  auto serverID = ServerID{"PRMR-1234"};
  auto scheduler = std::make_shared<MockScheduler>();
  auto dispatcher = std::make_shared<EmptyExternalDispatcher>();
  auto runtime = std::make_shared<MockRuntime>(serverID, "RuntimeTest",
                                               scheduler, dispatcher);

  auto pong_actor = runtime->spawn<pong_actor::Actor>(pong_actor::PongState{},
                                                      pong_actor::Start{});
  auto ping_actor = runtime->spawn<ping_actor::Actor>(
      ping_actor::PingState{},
      ping_actor::Start{.pongActor =
                            ActorPID{.server = serverID, .id = pong_actor}});

  auto ping_actor_state =
      runtime->getActorStateByID<ping_actor::Actor>(ping_actor);
  ASSERT_EQ(ping_actor_state,
            (ping_actor::PingState{.called = 2, .message = "hello world"}));
  auto pong_actor_state =
      runtime->getActorStateByID<pong_actor::Actor>(pong_actor);
  ASSERT_EQ(pong_actor_state, (pong_actor::PongState{.called = 2}));
}

TEST(ActorRuntimeTest, spawn_game) {
  auto serverID = ServerID{"PRMR-1234"};
  auto scheduler = std::make_shared<MockScheduler>();
  auto dispatcher = std::make_shared<EmptyExternalDispatcher>();
  auto runtime = std::make_shared<MockRuntime>(serverID, "RuntimeTest",
                                               scheduler, dispatcher);

  auto spawn_actor =
      runtime->spawn<SpawnActor>(SpawnState{}, SpawnStartMessage{});

  runtime->dispatch(ActorPID{.server = serverID, .id = spawn_actor},
                    ActorPID{.server = serverID, .id = spawn_actor},
                    SpawnActor::Message{SpawnMessage{"baz"}});

  ASSERT_EQ(runtime->getActorIDs().size(), 2);
  ASSERT_EQ(runtime->getActorStateByID<SpawnActor>(spawn_actor),
            (SpawnState{.called = 2, .state = "baz"}));
}

TEST(ActorRuntimeTest, finishes_actor_when_actor_says_so) {
  auto serverID = ServerID{"PRMR-1234"};
  auto scheduler = std::make_shared<MockScheduler>();
  auto dispatcher = std::make_shared<EmptyExternalDispatcher>();
  auto runtime = std::make_shared<MockRuntime>(serverID, "RuntimeTest",
                                               scheduler, dispatcher);

  auto finishing_actor =
      runtime->spawn<FinishingActor>(FinishingState{}, FinishingStart{});

  runtime->dispatch(ActorPID{.server = serverID, .id = finishing_actor},
                    ActorPID{.server = serverID, .id = finishing_actor},
                    FinishingActor::Message{FinishingFinish{}});

  ASSERT_TRUE(runtime->actors.find(finishing_actor)->get()->finishedAndIdle());
}

TEST(ActorRuntimeTest, garbage_collects_finished_actor) {
  auto serverID = ServerID{"PRMR-1234"};
  auto scheduler = std::make_shared<MockScheduler>();
  auto dispatcher = std::make_shared<EmptyExternalDispatcher>();
  auto runtime = std::make_shared<MockRuntime>(serverID, "RuntimeTest",
                                               scheduler, dispatcher);

  auto finishing_actor =
      runtime->spawn<FinishingActor>(FinishingState{}, FinishingStart{});

  runtime->dispatch(ActorPID{.server = serverID, .id = finishing_actor},
                    ActorPID{.server = serverID, .id = finishing_actor},
                    FinishingActor::Message{FinishingFinish{}});

  runtime->garbageCollect();

  ASSERT_EQ(runtime->actors.size(), 0);
}

TEST(ActorRuntimeTest, garbage_collects_all_finished_actors) {
  auto serverID = ServerID{"PRMR-1234"};
  auto scheduler = std::make_shared<MockScheduler>();
  auto dispatcher = std::make_shared<EmptyExternalDispatcher>();
  auto runtime = std::make_shared<MockRuntime>(serverID, "RuntimeTest",
                                               scheduler, dispatcher);

  auto actor_to_be_finished =
      runtime->spawn<FinishingActor>(FinishingState{}, FinishingStart{});
  runtime->spawn<FinishingActor>(FinishingState{}, FinishingStart{});
  runtime->spawn<FinishingActor>(FinishingState{}, FinishingStart{});
  auto another_actor_to_be_finished =
      runtime->spawn<FinishingActor>(FinishingState{}, FinishingStart{});
  runtime->spawn<FinishingActor>(FinishingState{}, FinishingStart{});

  runtime->dispatch(ActorPID{.server = serverID, .id = actor_to_be_finished},
                    ActorPID{.server = serverID, .id = actor_to_be_finished},
                    FinishingActor::Message{FinishingFinish{}});
  runtime->dispatch(
      ActorPID{.server = serverID, .id = another_actor_to_be_finished},
      ActorPID{.server = serverID, .id = another_actor_to_be_finished},
      FinishingActor::Message{FinishingFinish{}});

  runtime->garbageCollect();

  ASSERT_EQ(runtime->actors.size(), 3);
  auto remaining_actor_ids = runtime->getActorIDs();
  std::unordered_set<ActorID> actor_ids(remaining_actor_ids.begin(),
                                        remaining_actor_ids.end());
  ASSERT_FALSE(actor_ids.contains(actor_to_be_finished));
  ASSERT_FALSE(actor_ids.contains(another_actor_to_be_finished));
}

TEST(ActorRuntimeTest,
     finishes_and_garbage_collects_all_actors_when_shutting_down) {
  auto serverID = ServerID{"PRMR-1234"};
  auto scheduler = std::make_shared<MockScheduler>();
  auto dispatcher = std::make_shared<EmptyExternalDispatcher>();
  auto runtime = std::make_shared<MockRuntime>(serverID, "RuntimeTest",
                                               scheduler, dispatcher);
  runtime->spawn<TrivialActor>(TrivialState{}, TrivialStart{});
  runtime->spawn<TrivialActor>(TrivialState{}, TrivialStart{});
  runtime->spawn<TrivialActor>(TrivialState{}, TrivialStart{});
  runtime->spawn<TrivialActor>(TrivialState{}, TrivialStart{});
  runtime->spawn<TrivialActor>(TrivialState{}, TrivialStart{});
  ASSERT_EQ(runtime->actors.size(), 5);

  runtime->softShutdown();
  ASSERT_EQ(runtime->actors.size(), 0);
}
