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

#include <gtest/gtest.h>
#include <memory>
#include "fmt/core.h"
#include "velocypack/SharedSlice.h"
#include "Inspection/VPackWithErrorT.h"

#include "Actor/Message.h"
#include "Actor/MPSCQueue.h"
#include "Actor/Runtime.h"
#include "Actor/Actor.h"
#include "Actor/ActorPID.h"

#include "Actors/TrivialActor.h"
#include "ThreadPoolScheduler.h"

using namespace arangodb::pregel::actor;
using namespace arangodb::pregel::actor::test;

struct MockScheduler {
  auto start(size_t number_of_threads) -> void{};
  auto stop() -> void{};
  auto operator()(auto fn) { fn(); }
};
struct EmptyExternalDispatcher {
  auto operator()(ActorPID sender, ActorPID receiver,
                  arangodb::velocypack::SharedSlice msg) -> void {}
};
using ActorTestRuntime = Runtime<MockScheduler, EmptyExternalDispatcher>;

template<typename T>
class ActorTest : public testing::Test {
 public:
  ActorTest() : scheduler{std::make_shared<T>()} {
    scheduler->start(number_of_threads);
  }

  std::shared_ptr<T> scheduler;
  size_t number_of_threads = 128;
};
using SchedulerTypes = ::testing::Types<MockScheduler, ThreadPoolScheduler>;
TYPED_TEST_SUITE(ActorTest, SchedulerTypes);

TEST(ActorTest, has_a_type_name) {
  auto scheduler = std::make_shared<MockScheduler>();
  auto dispatcher = std::make_shared<EmptyExternalDispatcher>();
  auto runtime =
      std::make_shared<ActorTestRuntime>("A", "myID", scheduler, dispatcher);
  auto actor = std::make_shared<Actor<ActorTestRuntime, TrivialActor>>(
      ActorPID{}, runtime, std::make_unique<TrivialState>());
  ASSERT_EQ(actor->typeName(), "TrivialActor");
}

TEST(ActorTest, formats_actor) {
  auto scheduler = std::make_shared<MockScheduler>();
  auto dispatcher = std::make_shared<EmptyExternalDispatcher>();
  auto runtime =
      std::make_shared<ActorTestRuntime>("A", "myID", scheduler, dispatcher);
  auto actor = std::make_shared<Actor<ActorTestRuntime, TrivialActor>>(
      ActorPID{.server = "A", .database = "database", .id = {1}}, runtime,
      std::make_unique<TrivialState>());
  ASSERT_EQ(
      fmt::format("{}", *actor),
      R"({"pid":{"server":"A","database":"database","id":1},"state":{"state":"","called":0},"batchsize":16})");
}

TEST(ActorTest, changes_its_state_after_processing_a_message) {
  auto scheduler = std::make_shared<MockScheduler>();
  auto dispatcher = std::make_shared<EmptyExternalDispatcher>();
  auto runtime =
      std::make_shared<ActorTestRuntime>("A", "myID", scheduler, dispatcher);
  auto actor = std::make_shared<Actor<ActorTestRuntime, TrivialActor>>(
      ActorPID{.server = "A", .database = "database", .id = {1}}, runtime,
      std::make_unique<TrivialState>());
  ASSERT_EQ(actor->getState(), (TrivialState{.state = "", .called = 0}));

  auto message = MessagePayload<TrivialMessages>(TrivialMessage{"Hello"});
  actor->process(ActorPID{.server = "A", .database = "database", .id = {5}},
                 message);
  ASSERT_EQ(actor->getState(), (TrivialState{.state = "Hello", .called = 1}));
}

TEST(ActorTest, changes_its_state_after_processing_a_velocypack_message) {
  auto scheduler = std::make_shared<MockScheduler>();
  auto dispatcher = std::make_shared<EmptyExternalDispatcher>();
  auto runtime =
      std::make_shared<ActorTestRuntime>("A", "myID", scheduler, dispatcher);
  auto actor = std::make_shared<Actor<ActorTestRuntime, TrivialActor>>(
      ActorPID{.server = "A", .database = "database", .id = {1}}, runtime,
      std::make_unique<TrivialState>());
  ASSERT_EQ(actor->getState(), (TrivialState{.state = "", .called = 0}));

  auto message = TrivialMessages{TrivialMessage{"Hello"}};
  actor->process(ActorPID{.server = "A", .database = "database", .id = {5}},
                 arangodb::inspection::serializeWithErrorT(message).get());

  ASSERT_EQ(actor->getState(), (TrivialState{.state = "Hello", .called = 1}));
}

TEST(ActorTest, sets_itself_to_finish) {
  auto scheduler = std::make_shared<MockScheduler>();
  auto dispatcher = std::make_shared<EmptyExternalDispatcher>();
  auto runtime =
      std::make_shared<ActorTestRuntime>("A", "myID", scheduler, dispatcher);
  auto actor = std::make_shared<Actor<ActorTestRuntime, TrivialActor>>(
      ActorPID{.server = "A", .database = "database", .id = {1}}, runtime,
      std::make_unique<TrivialState>());
  ASSERT_FALSE(actor->isFinishedAndIdle());

  actor->finish();

  ASSERT_TRUE(actor->isFinishedAndIdle());
}

TYPED_TEST(ActorTest, does_not_work_on_new_messages_after_actor_finished) {
  auto dispatcher = std::make_shared<EmptyExternalDispatcher>();
  auto runtime = std::make_shared<Runtime<TypeParam, EmptyExternalDispatcher>>(
      "A", "myID", this->scheduler, dispatcher);
  auto actor = std::make_shared<
      Actor<Runtime<TypeParam, EmptyExternalDispatcher>, TrivialActor>>(
      ActorPID{.server = "A", .database = "database", .id = {1}}, runtime,
      std::make_unique<TrivialState>());
  actor->finish();

  // send message to actor
  auto message = TrivialMessages{TrivialMessage{"Hello"}};
  actor->process(ActorPID{.server = "A", .database = "database", .id = {5}},
                 arangodb::inspection::serializeWithErrorT(message).get());

  this->scheduler->stop();
  // actor did not receive message
  ASSERT_EQ(actor->getState(), (TrivialState{}));
}

TYPED_TEST(ActorTest, finished_actor_works_on_all_remaining_messages_in_queue) {
  auto dispatcher = std::make_shared<EmptyExternalDispatcher>();
  auto runtime = std::make_shared<Runtime<TypeParam, EmptyExternalDispatcher>>(
      "A", "myID", this->scheduler, dispatcher);
  auto actor = std::make_shared<
      Actor<Runtime<TypeParam, EmptyExternalDispatcher>, TrivialActor>>(
      ActorPID{.server = "A", .database = "database", .id = {1}}, runtime,
      std::make_unique<TrivialState>());

  // send a lot of messages to actor
  auto message = TrivialMessages{TrivialMessage{"A"}};
  size_t sent_message_count = 1000;
  for (size_t i = 0; i < sent_message_count; i++) {
    actor->process(ActorPID{.server = "A", .database = "database", .id = {5}},
                   arangodb::inspection::serializeWithErrorT(message).get());
  }

  // finish actor possibly before actor worked on all messages
  actor->finish();

  // wait for actor to work off all messages
  while (not actor->isIdle()) {
  }
  this->scheduler->stop();
  ASSERT_EQ(actor->getState(),
            (TrivialState{.state = std::string(sent_message_count, 'A'),
                          .called = sent_message_count}));
}
