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
#include "Pregel/Actor/Runtime.h"

#include "Pregel/Actor/Actors/TrivialActor.h"
#include "Pregel/Actor/Actors/PingPongActors.h"

using namespace arangodb;

using namespace arangodb::pregel::actor;
using namespace arangodb::pregel::actor::test;

struct MockScheduler {
  auto operator()(auto fn) { fn(); }
};

template<typename Runtime>
struct MockExternalDispatcher {
  MockExternalDispatcher(
      std::unordered_map<ServerID, std::shared_ptr<Runtime>>& runtimes)
      : runtimes(runtimes) {}
  auto operator()(ActorPID sender, ActorPID receiver,
                  arangodb::velocypack::SharedSlice msg) -> void {
    // a timeout error would come here
    auto receiving_runtime = runtimes.find(receiver.server);
    if (receiving_runtime != std::end(runtimes)) {
      receiving_runtime->second->receive(sender, receiver, msg);
    } else {
      auto error = ActorError{ActorNotFound{.actor = receiver}};
      auto payload = inspection::serializeWithErrorT(error);
      if (payload.ok()) {
        runtimes[sender.server]->dispatch(receiver, sender, payload.get());
      } else {
        fmt::print("Error serializing ActorNotFound");
        std::abort();
      }
    }
  }
  std::unordered_map<ServerID, std::shared_ptr<Runtime>>& runtimes;
};

struct MockRuntime
    : Runtime<MockScheduler, MockExternalDispatcher<MockRuntime>> {
  using Runtime<MockScheduler, MockExternalDispatcher<MockRuntime>>::Runtime;
};

TEST(MultiRuntimeTest, sends_message_to_actor_in_another_runtime) {
  std::unordered_map<ServerID, std::shared_ptr<MockRuntime>> runtimes;
  auto scheduler = std::make_shared<MockScheduler>();
  auto dispatcher =
      std::make_shared<MockExternalDispatcher<MockRuntime>>(runtimes);

  // Sending Runtime
  auto sending_server = ServerID{"A"};
  runtimes.emplace(sending_server, std::make_shared<MockRuntime>(
                                       sending_server, "RuntimeTest-sending",
                                       scheduler, dispatcher));
  auto sending_actor_id = runtimes[sending_server]->spawn<TrivialActor>(
      TrivialState{.state = "foo"}, TrivialStart{});
  auto sending_actor =
      ActorPID{.server = sending_server, .id = sending_actor_id};

  // Receiving Runtime
  auto recieving_server = ServerID{"B"};
  runtimes.emplace(
      recieving_server,
      std::make_shared<MockRuntime>(recieving_server, "RuntimeTest-receiving",
                                    scheduler, dispatcher));
  auto recieving_actor_id = runtimes[recieving_server]->spawn<TrivialActor>(
      TrivialState{.state = "foo"}, TrivialStart{});
  auto recieving_actor =
      ActorPID{.server = recieving_server, .id = recieving_actor_id};

  // send
  runtimes[sending_server]->dispatch(
      sending_actor, recieving_actor,
      TrivialActor::Message{TrivialMessage("baz")});

  // sending actor state did not change
  auto sending_actor_state =
      runtimes[sending_server]->getActorStateByID<TrivialActor>(
          sending_actor_id);
  ASSERT_EQ(sending_actor_state,
            (TrivialActor::State{.state = "foo", .called = 1}));
  // receiving actor state changed
  auto recieving_actor_state =
      runtimes[recieving_server]->getActorStateByID<TrivialActor>(
          recieving_actor_id);
  ASSERT_EQ(recieving_actor_state,
            (TrivialActor::State{.state = "foobaz", .called = 2}));
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
TEST(MultiRuntimeTest,
     actor_receiving_wrong_message_type_sends_back_unknown_error_message) {
  std::unordered_map<ServerID, std::shared_ptr<MockRuntime>> runtimes;
  auto scheduler = std::make_shared<MockScheduler>();
  auto dispatcher =
      std::make_shared<MockExternalDispatcher<MockRuntime>>(runtimes);

  // Sending Runtime
  auto sending_server = ServerID{"A"};
  runtimes.emplace(sending_server, std::make_shared<MockRuntime>(
                                       sending_server, "RuntimeTest-sending",
                                       scheduler, dispatcher));
  auto sending_actor_id = runtimes[sending_server]->spawn<TrivialActor>(
      TrivialState{.state = "foo"}, TrivialStart{});
  auto sending_actor =
      ActorPID{.server = sending_server, .id = sending_actor_id};

  // Receiving Runtime
  auto recieving_server = ServerID{"B"};
  runtimes.emplace(
      recieving_server,
      std::make_shared<MockRuntime>(recieving_server, "RuntimeTest-receiving",
                                    scheduler, dispatcher));
  auto recieving_actor_id = runtimes[recieving_server]->spawn<TrivialActor>(
      TrivialState{.state = "foo"}, TrivialStart{});
  auto recieving_actor =
      ActorPID{.server = recieving_server, .id = recieving_actor_id};

  // send
  runtimes[sending_server]->dispatch(sending_actor, recieving_actor,
                                     SomeMessages{SomeMessage{}});

  // recieving actor state was called once
  auto recieving_actor_state =
      runtimes[recieving_server]->getActorStateByID<TrivialActor>(
          recieving_actor_id);
  ASSERT_EQ(recieving_actor_state,
            (TrivialActor::State{.state = "foo", .called = 1}));
  // sending actor received an unknown message error after it sent wrong
  // message type
  auto sending_actor_state =
      runtimes[sending_server]->getActorStateByID<TrivialActor>(
          sending_actor_id);
  ASSERT_EQ(
      sending_actor_state,
      (TrivialActor::State{
          .state = fmt::format("sent unknown message to {}", recieving_actor),
          .called = 2}));
}

// TODO extra type of error: ServerNotFound?
TEST(
    MultiRuntimeTest,
    actor_receives_actor_not_found_message_after_trying_to_send_message_to_non_existent_server) {
  std::unordered_map<ServerID, std::shared_ptr<MockRuntime>> runtimes;
  auto scheduler = std::make_shared<MockScheduler>();
  auto dispatcher =
      std::make_shared<MockExternalDispatcher<MockRuntime>>(runtimes);

  // Sending Runtime
  auto sending_server = ServerID{"A"};
  runtimes.emplace(sending_server, std::make_shared<MockRuntime>(
                                       sending_server, "RuntimeTest-sending",
                                       scheduler, dispatcher));
  auto sending_actor_id = runtimes[sending_server]->spawn<TrivialActor>(
      TrivialState{.state = "foo"}, TrivialStart{});
  auto sending_actor =
      ActorPID{.server = sending_server, .id = sending_actor_id};

  // send
  auto actor_in_non_existing_runtime = ActorPID{.server = "B", .id = {999}};
  runtimes[sending_server]->dispatch(
      sending_actor, actor_in_non_existing_runtime,
      TrivialActor::Message{TrivialMessage("baz")});

  // sending actor received an actor not known message error after it messaged
  // to non-existing runtime
  ASSERT_EQ(
      runtimes[sending_server]->getActorStateByID<TrivialActor>(
          sending_actor_id),
      (TrivialActor::State{.state = fmt::format("recieving actor {} not found",
                                                actor_in_non_existing_runtime),
                           .called = 2}));
}

TEST(
    MultiRuntimeTest,
    actor_receives_actor_not_found_message_after_trying_to_send_message_to_non_existent_actor_on_another_server) {
  std::unordered_map<ServerID, std::shared_ptr<MockRuntime>> runtimes;
  auto scheduler = std::make_shared<MockScheduler>();
  auto dispatcher =
      std::make_shared<MockExternalDispatcher<MockRuntime>>(runtimes);

  // Sending Runtime
  auto sending_server = ServerID{"A"};
  runtimes.emplace(sending_server, std::make_shared<MockRuntime>(
                                       sending_server, "RuntimeTest-sending",
                                       scheduler, dispatcher));
  auto sending_actor_id = runtimes[sending_server]->spawn<TrivialActor>(
      TrivialState{.state = "foo"}, TrivialStart{});
  auto sending_actor =
      ActorPID{.server = sending_server, .id = sending_actor_id};

  // Receiving Runtime
  auto recieving_server = ServerID{"B"};
  runtimes.emplace(
      recieving_server,
      std::make_shared<MockRuntime>(recieving_server, "RuntimeTest-receiving",
                                    scheduler, dispatcher));

  // send
  auto unknown_actor = ActorPID{.server = recieving_server, .id = {999}};
  runtimes[sending_server]->dispatch(
      sending_actor, unknown_actor,
      TrivialActor::Message{TrivialMessage("baz")});

  // sending actor received an actor not known message error after it messaged
  // to non-existing actor on other runtime
  ASSERT_EQ(
      runtimes[sending_server]->getActorStateByID<TrivialActor>(
          sending_actor_id),
      (TrivialActor::State{
          .state = fmt::format("recieving actor {} not found", unknown_actor),
          .called = 2}));
}

TEST(MultiRuntimeTest, ping_pong_game) {
  std::unordered_map<ServerID, std::shared_ptr<MockRuntime>> runtimes;

  auto scheduler = std::make_shared<MockScheduler>();
  auto dispatcher =
      std::make_shared<MockExternalDispatcher<MockRuntime>>(runtimes);

  // Pong Runtime
  auto pong_server = ServerID{"A"};
  runtimes.emplace(pong_server,
                   std::make_shared<MockRuntime>(pong_server, "RuntimeTest-A",
                                                 scheduler, dispatcher));
  auto pong_actor = runtimes[pong_server]->spawn<pong_actor::Actor>(
      pong_actor::State{}, pong_actor::Start{});

  // Ping Runtime
  auto ping_server = ServerID{"B"};
  runtimes.emplace(ping_server,
                   std::make_shared<MockRuntime>(ping_server, "RuntimeTest-B",
                                                 scheduler, dispatcher));
  auto ping_actor = runtimes[ping_server]->spawn<ping_actor::Actor>(
      ping_actor::State{},
      ping_actor::Start{.pongActor =
                            ActorPID{.server = pong_server, .id = pong_actor}});

  // pong actor was called twice
  auto pong_actor_state =
      runtimes[pong_server]->getActorStateByID<pong_actor::Actor>(pong_actor);
  ASSERT_EQ(pong_actor_state, (pong_actor::State{.called = 2}));
  // ping actor received message from pong
  auto ping_actor_state =
      runtimes[ping_server]->getActorStateByID<ping_actor::Actor>(ping_actor);
  ASSERT_EQ(ping_actor_state,
            (ping_actor::State{.called = 2, .message = "hello world"}));
}
