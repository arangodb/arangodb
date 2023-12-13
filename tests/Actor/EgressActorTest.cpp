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

#include "Actor/DistributedRuntime.h"
#include "Actors/EgressActor.h"
#include "MockScheduler.h"
#include "ThreadPoolScheduler.h"

using namespace arangodb::actor;
using namespace arangodb::actor::test;

struct EmptyExternalDispatcher : IExternalDispatcher {
  void dispatch(DistributedActorPID sender, DistributedActorPID receiver,
                arangodb::velocypack::SharedSlice msg) override {}
};
using ActorTestRuntime = DistributedRuntime;

template<typename T>
class EgressActorTest : public testing::Test {
 public:
  EgressActorTest() : scheduler{std::make_shared<T>()} {
    scheduler->start(number_of_threads);
  }

  std::shared_ptr<T> scheduler;
  size_t number_of_threads = 128;
};
using SchedulerTypes = ::testing::Types<MockScheduler, ThreadPoolScheduler>;
TYPED_TEST_SUITE(EgressActorTest, SchedulerTypes);

TYPED_TEST(EgressActorTest,
           outside_world_can_look_at_set_data_inside_egress_actor) {
  auto dispatcher = std::make_shared<EmptyExternalDispatcher>();
  auto runtime = std::make_shared<DistributedRuntime>(
      "A", "myID", this->scheduler, dispatcher);

  auto actorState = std::make_unique<EgressState>();

  // keep a shared pointer to the outbox
  auto outbox = actorState->data;

  auto actor = runtime->template spawn<EgressActor>(
      std::move(actorState), test::message::EgressStart{});

  runtime->dispatch(
      actor, actor,
      EgressActor::Message{test::message::EgressSet{.data = "Hallo"}});

  this->scheduler->stop();

  ASSERT_EQ(outbox->get(), "Hallo");
  runtime->softShutdown();
}

TYPED_TEST(EgressActorTest, egress_data_is_empty_when_not_set) {
  auto dispatcher = std::make_shared<EmptyExternalDispatcher>();
  auto runtime = std::make_shared<DistributedRuntime>(
      "A", "myID", this->scheduler, dispatcher);

  auto actorState = std::make_unique<EgressState>();

  // keep a shared pointer to the outbox
  auto outbox = actorState->data;

  runtime->template spawn<EgressActor>(std::move(actorState),
                                       test::message::EgressStart{});

  this->scheduler->stop();

  ASSERT_EQ(outbox->get(), std::nullopt);
  runtime->softShutdown();
}
