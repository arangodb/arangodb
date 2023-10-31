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

#include "Actors/TrivialActor.h"
#include "Actors/PingPongActors.h"
#include "ThreadPoolScheduler.h"

using namespace arangodb;

using namespace arangodb::pregel::actor;
using namespace arangodb::pregel::actor::test;

struct MockScheduler {
  auto start(size_t number_of_threads) -> void{};
  auto stop() -> void{};
  auto operator()(auto fn) { fn(); }
  auto delay(std::chrono::seconds delay, std::function<void(bool)>&& fn) {
    fn(true);
  }
};

template<typename Runtime>
struct MockExternalDispatcher {
  MockExternalDispatcher(
      std::unordered_map<ServerID, std::shared_ptr<Runtime>>& runtimes)
      : runtimes(runtimes) {}
  auto operator()(ActorPID sender, ActorPID receiver,
                  arangodb::velocypack::SharedSlice msg) -> void {
    auto receiving_runtime = runtimes.find(receiver.server);
    if (receiving_runtime != std::end(runtimes)) {
      receiving_runtime->second->receive(sender, receiver, msg);
    } else {
      auto error = pregel::actor::message::ActorError{
          pregel::actor::message::NetworkError{
              .message =
                  fmt::format("Cannot find server {}", receiver.server)}};
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

template<typename T>
class ActorMultiRuntimeTest : public testing::Test {
 public:
  ActorMultiRuntimeTest() : scheduler{std::make_shared<T>()} {
    scheduler->start(number_of_threads);
  }
  struct MockRuntime : Runtime<T, MockExternalDispatcher<MockRuntime>> {
    using Runtime<T, MockExternalDispatcher<MockRuntime>>::Runtime;
  };

  std::shared_ptr<T> scheduler;
  size_t number_of_threads = 128;
};
using SchedulerTypes = ::testing::Types<MockScheduler, ThreadPoolScheduler>;
TYPED_TEST_SUITE(ActorMultiRuntimeTest, SchedulerTypes);

TYPED_TEST(ActorMultiRuntimeTest, sends_message_to_actor_in_another_runtime) {
  std::unordered_map<ServerID,
                     std::shared_ptr<typename TestFixture::MockRuntime>>
      runtimes;
  auto dispatcher = std::make_shared<
      MockExternalDispatcher<typename TestFixture::MockRuntime>>(runtimes);

  // Sending Runtime
  auto sending_server = ServerID{"A"};
  runtimes.emplace(
      sending_server,
      std::make_shared<typename TestFixture::MockRuntime>(
          sending_server, "RuntimeTest-sending", this->scheduler, dispatcher));
  auto sending_actor_id =
      runtimes[sending_server]->template spawn<TrivialActor>(
          "database", std::make_unique<TrivialState>("foo"),
          test::message::TrivialStart{});
  auto sending_actor = ActorPID{
      .server = sending_server, .database = "database", .id = sending_actor_id};

  // Receiving Runtime
  auto receiving_server = ServerID{"B"};
  runtimes.emplace(receiving_server,
                   std::make_shared<typename TestFixture::MockRuntime>(
                       receiving_server, "RuntimeTest-receiving",
                       this->scheduler, dispatcher));
  auto receiving_actor_id =
      runtimes[receiving_server]->template spawn<TrivialActor>(
          "database", std::make_unique<TrivialState>("foo"),
          test::message::TrivialStart{});
  auto receiving_actor = ActorPID{.server = receiving_server,
                                  .database = "database",
                                  .id = receiving_actor_id};

  // send
  runtimes[sending_server]->dispatch(
      sending_actor, receiving_actor,
      TrivialActor::Message{test::message::TrivialMessage("baz")});

  this->scheduler->stop();
  // sending actor state did not change
  auto sending_actor_state =
      runtimes[sending_server]->template getActorStateByID<TrivialActor>(
          sending_actor_id);
  ASSERT_EQ(sending_actor_state, (TrivialActor::State("foo", 1)));
  // receiving actor state changed
  auto receiving_actor_state =
      runtimes[receiving_server]->template getActorStateByID<TrivialActor>(
          receiving_actor_id);
  ASSERT_EQ(receiving_actor_state, (TrivialActor::State("foobaz", 2)));
  for (auto& [_, runtime] : runtimes) {
    runtime->softShutdown();
  }
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
TYPED_TEST(
    ActorMultiRuntimeTest,
    actor_receiving_wrong_message_type_sends_back_unknown_error_message) {
  std::unordered_map<ServerID,
                     std::shared_ptr<typename TestFixture::MockRuntime>>
      runtimes;
  auto dispatcher = std::make_shared<
      MockExternalDispatcher<typename TestFixture::MockRuntime>>(runtimes);

  // Sending Runtime
  auto sending_server = ServerID{"A"};
  runtimes.emplace(
      sending_server,
      std::make_shared<typename TestFixture::MockRuntime>(
          sending_server, "RuntimeTest-sending", this->scheduler, dispatcher));
  auto sending_actor_id =
      runtimes[sending_server]->template spawn<TrivialActor>(
          "database", std::make_unique<TrivialState>("foo"),
          test::message::TrivialStart{});
  auto sending_actor = ActorPID{
      .server = sending_server, .database = "database", .id = sending_actor_id};

  // Receiving Runtime
  auto receiving_server = ServerID{"B"};
  runtimes.emplace(receiving_server,
                   std::make_shared<typename TestFixture::MockRuntime>(
                       receiving_server, "RuntimeTest-receiving",
                       this->scheduler, dispatcher));
  auto receiving_actor_id =
      runtimes[receiving_server]->template spawn<TrivialActor>(
          "database", std::make_unique<TrivialState>("foo"),
          test::message::TrivialStart{});
  auto receiving_actor = ActorPID{.server = receiving_server,
                                  .database = "database",
                                  .id = receiving_actor_id};

  // send
  runtimes[sending_server]->dispatch(sending_actor, receiving_actor,
                                     SomeMessages{SomeMessage{}});

  this->scheduler->stop();
  // receiving actor state was called once
  auto receiving_actor_state =
      runtimes[receiving_server]->template getActorStateByID<TrivialActor>(
          receiving_actor_id);
  ASSERT_EQ(receiving_actor_state, (TrivialActor::State("foo", 1)));
  // sending actor received an unknown message error after it sent wrong
  // message type
  auto sending_actor_state =
      runtimes[sending_server]->template getActorStateByID<TrivialActor>(
          sending_actor_id);
  ASSERT_EQ(
      sending_actor_state,
      (TrivialActor::State(
          fmt::format("sent unknown message to {}", receiving_actor), 2)));
  for (auto& [_, runtime] : runtimes) {
    runtime->softShutdown();
  }
}

TYPED_TEST(
    ActorMultiRuntimeTest,
    actor_receives_actor_not_found_message_after_trying_to_send_message_to_non_existent_actor) {
  std::unordered_map<ServerID,
                     std::shared_ptr<typename TestFixture::MockRuntime>>
      runtimes;
  auto dispatcher = std::make_shared<
      MockExternalDispatcher<typename TestFixture::MockRuntime>>(runtimes);

  // Sending Runtime
  auto sending_server = ServerID{"A"};
  runtimes.emplace(
      sending_server,
      std::make_shared<typename TestFixture::MockRuntime>(
          sending_server, "RuntimeTest-sending", this->scheduler, dispatcher));
  auto sending_actor_id =
      runtimes[sending_server]->template spawn<TrivialActor>(
          "database", std::make_unique<TrivialState>("foo"),
          test::message::TrivialStart{});
  auto sending_actor = ActorPID{
      .server = sending_server, .database = "database", .id = sending_actor_id};

  // Receiving Runtime
  auto receiving_server = ServerID{"B"};
  runtimes.emplace(receiving_server,
                   std::make_shared<typename TestFixture::MockRuntime>(
                       receiving_server, "RuntimeTest-receiving",
                       this->scheduler, dispatcher));

  // send
  auto unknown_actor =
      ActorPID{.server = receiving_server, .database = "database", .id = {999}};
  runtimes[sending_server]->dispatch(
      sending_actor, unknown_actor,
      TrivialActor::Message{test::message::TrivialMessage("baz")});

  this->scheduler->stop();
  // sending actor received an actor-not-known message error
  // after it messaged to non-existing actor on other runtime
  ASSERT_EQ(
      runtimes[sending_server]->template getActorStateByID<TrivialActor>(
          sending_actor_id),
      (TrivialActor::State(
          fmt::format("receiving actor {} not found", unknown_actor), 2)));
  for (auto& [_, runtime] : runtimes) {
    runtime->softShutdown();
  }
}

TYPED_TEST(
    ActorMultiRuntimeTest,
    actor_receives_network_error_message_after_trying_to_send_message_to_non_existent_server) {
  std::unordered_map<ServerID,
                     std::shared_ptr<typename TestFixture::MockRuntime>>
      runtimes;
  auto dispatcher = std::make_shared<
      MockExternalDispatcher<typename TestFixture::MockRuntime>>(runtimes);

  // Sending Runtime
  auto sending_server = ServerID{"A"};
  runtimes.emplace(
      sending_server,
      std::make_shared<typename TestFixture::MockRuntime>(
          sending_server, "RuntimeTest-sending", this->scheduler, dispatcher));
  auto sending_actor_id =
      runtimes[sending_server]->template spawn<TrivialActor>(
          "database", std::make_unique<TrivialState>("foo"),
          test::message::TrivialStart{});
  auto sending_actor = ActorPID{
      .server = sending_server, .database = "database", .id = sending_actor_id};

  // send
  auto unknown_server = ServerID{"B"};
  runtimes[sending_server]->dispatch(
      sending_actor,
      ActorPID{.server = unknown_server, .database = "database", .id = {999}},
      TrivialActor::Message{test::message::TrivialMessage("baz")});

  this->scheduler->stop();
  // sending actor received a network error message after it messaged
  // to non-existing server
  ASSERT_EQ(
      runtimes[sending_server]->template getActorStateByID<TrivialActor>(
          sending_actor_id),
      (TrivialActor::State(
          fmt::format("network error: Cannot find server {}", unknown_server),
          2)));
  for (auto& [_, runtime] : runtimes) {
    runtime->softShutdown();
  }
}

TYPED_TEST(ActorMultiRuntimeTest, ping_pong_game) {
  std::unordered_map<ServerID,
                     std::shared_ptr<typename TestFixture::MockRuntime>>
      runtimes;

  auto dispatcher = std::make_shared<
      MockExternalDispatcher<typename TestFixture::MockRuntime>>(runtimes);

  // Pong Runtime
  auto pong_server = ServerID{"A"};
  runtimes.emplace(
      pong_server,
      std::make_shared<typename TestFixture::MockRuntime>(
          pong_server, "RuntimeTest-A", this->scheduler, dispatcher));
  auto pong_actor = runtimes[pong_server]->template spawn<pong_actor::Actor>(
      "database", std::make_unique<pong_actor::PongState>(),
      pong_actor::message::Start{});

  // Ping Runtime
  auto ping_server = ServerID{"B"};
  runtimes.emplace(
      ping_server,
      std::make_shared<typename TestFixture::MockRuntime>(
          ping_server, "RuntimeTest-B", this->scheduler, dispatcher));
  auto ping_actor = runtimes[ping_server]->template spawn<ping_actor::Actor>(
      "database", std::make_unique<ping_actor::PingState>(),
      ping_actor::message::Start{.pongActor = ActorPID{.server = pong_server,
                                                       .database = "database",
                                                       .id = pong_actor}});

  this->scheduler->stop();
  // pong actor was called twice
  auto pong_actor_state =
      runtimes[pong_server]->template getActorStateByID<pong_actor::Actor>(
          pong_actor);
  ASSERT_EQ(pong_actor_state, (pong_actor::PongState{.called = 2}));
  // ping actor received message from pong
  auto ping_actor_state =
      runtimes[ping_server]->template getActorStateByID<ping_actor::Actor>(
          ping_actor);
  ASSERT_EQ(ping_actor_state,
            (ping_actor::PingState{.called = 2, .message = "hello world"}));
  for (auto& [_, runtime] : runtimes) {
    runtime->softShutdown();
  }
}
