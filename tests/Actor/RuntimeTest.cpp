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
#include <memory>
#include <type_traits>
#include <unordered_set>
#include "fmt/format.h"
#include "velocypack/SharedSlice.h"
#include "VelocypackUtils/VelocyPackStringLiteral.h"

#include "Actor/IScheduler.h"
#include "Actor/DistributedRuntime.h"
#include "Actor/LocalRuntime.h"

#include "Actors/FinishingActor.h"
#include "Actors/MonitoringActor.h"
#include "Actors/PingPongActors.h"
#include "Actors/SpawnActor.h"
#include "Actors/TrivialActor.h"
#include "MockScheduler.h"
#include "ThreadPoolScheduler.h"

using namespace arangodb::actor;
using namespace arangodb::actor::test;

namespace {
void waitForAllMessagesToBeProcessed(auto& runtime, auto& scheduler) {
  // the finish message is processed asynchronously, and the idle flag is set
  // before the actor is removed and the down messages are dispatched.
  // to ensure that all messages are fully processed, we not only have to check
  // the actors, but also make sure that the schedule itself is idle. And we
  // have to do this in an atomic way to avoid races.
  while (not scheduler.isIdle([&]() { return runtime->areAllActorsIdle(); })) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}
}  // namespace

struct EmptyExternalDispatcher : IExternalDispatcher {
  void dispatch(DistributedActorPID sender, DistributedActorPID receiver,
                arangodb::velocypack::SharedSlice msg) override {}
};

template<typename TRuntime, typename TScheduler>
struct Params {
  using Runtime = TRuntime;
  using Scheduler = TScheduler;
};

template<typename TRuntime>
struct RuntimeFactor;

template<>
struct RuntimeFactor<DistributedRuntime> {
  static ServerID serverId() { return "PRMR-1234"; }

  static auto create(std::shared_ptr<IScheduler> scheduler)
      -> std::shared_ptr<DistributedRuntime> {
    auto dispatcher = std::make_shared<EmptyExternalDispatcher>();
    return std::make_shared<DistributedRuntime>(
        serverId(), "DistributedRuntimeTest", scheduler, dispatcher);
  }

  static auto makePid(ActorID id) {
    return DistributedActorPID{
        .server = serverId(), .database = "database", .id = id};
  }
};

template<>
struct RuntimeFactor<LocalRuntime> {
  static auto create(std::shared_ptr<IScheduler> scheduler)
      -> std::shared_ptr<LocalRuntime> {
    return std::make_shared<LocalRuntime>("LocalRuntimeTest", scheduler);
  }

  static auto makePid(ActorID id) { return LocalActorPID{.id = id}; }
};

template<typename TParams>
class RuntimeTest : public testing::Test {
 public:
  using Scheduler = typename TParams::Scheduler;
  using TRuntime = typename TParams::Runtime;
  using ActorPID = typename TRuntime::ActorPID;

  RuntimeTest() : scheduler{std::make_shared<Scheduler>()} {
    scheduler->start(number_of_threads);
  }

  auto makeRuntime() -> std::shared_ptr<TRuntime> {
    return RuntimeFactor<TRuntime>::create(scheduler);
  }

  auto makePid(ActorID id) -> ActorPID {
    return RuntimeFactor<TRuntime>::makePid(id);
  }

  std::shared_ptr<Scheduler> scheduler;
  size_t number_of_threads = 128;
};

using Types = ::testing::Types<Params<DistributedRuntime, MockScheduler>,
                               Params<DistributedRuntime, ThreadPoolScheduler>,
                               Params<LocalRuntime, MockScheduler>,
                               Params<LocalRuntime, ThreadPoolScheduler>>;
TYPED_TEST_SUITE(RuntimeTest, Types);

TYPED_TEST(RuntimeTest, formats_runtime_and_actor_state) {
  auto runtime = this->makeRuntime();

  auto actorID = runtime->template spawn<pong_actor::Actor>(
      std::make_unique<pong_actor::PongState>(), pong_actor::message::Start{});

  this->scheduler->stop();
  auto expected = []() {
    if constexpr (std::is_same_v<typename TestFixture::TRuntime,
                                 DistributedRuntime>) {
      return R"x({"myServerID":"PRMR-1234","runtimeID":"DistributedRuntimeTest","uniqueActorIDCounter":2,"actors":{"ActorID(1)":{"type":"PongActor","monitors":[]}}})x";
    } else {
      return R"x({"runtimeID":"LocalRuntimeTest","uniqueActorIDCounter":2,"actors":{"ActorID(1)":{"type":"PongActor","monitors":[]}}})x";
    }
  }();

  ASSERT_EQ(
      fmt::format(
          "{}", arangodb::inspection::json(
                    *runtime, arangodb::inspection::JsonPrintFormat::kMinimal)),
      expected);

  auto actor =
      runtime->template getActorStateByID<pong_actor::Actor>(actorID).value();
  ASSERT_EQ(fmt::format("{}", actor), R"({"called":1})");
  runtime->softShutdown();
}

TYPED_TEST(RuntimeTest, serializes_an_actor_including_its_actor_state) {
  auto runtime = this->makeRuntime();
  auto actor = runtime->template spawn<TrivialActor>(
      std::make_unique<TrivialState>("foo"), test::message::TrivialStart());

  this->scheduler->stop();
  using namespace arangodb::velocypack;
  auto expected = []() {
    if constexpr (std::is_same_v<typename TestFixture::TRuntime,
                                 DistributedRuntime>) {
      return R"({"pid":{"server":"PRMR-1234","database":"database","id":1},"state":{"state":"foo","called":1},"batchsize":16})"_vpack;
    } else {
      return R"({"pid":{"id":1},"state":{"state":"foo","called":1},"batchsize":16})"_vpack;
    }
  }();
  ASSERT_EQ(runtime->getSerializedActorByID(actor.id)->toJson(),
            expected.toJson());
  runtime->softShutdown();
}

TYPED_TEST(RuntimeTest, spawns_actor) {
  auto runtime = this->makeRuntime();

  auto actor = runtime->template spawn<TrivialActor>(
      std::make_unique<TrivialState>("foo"), test::message::TrivialStart());

  this->scheduler->stop();
  auto state = runtime->template getActorStateByID<TrivialActor>(actor);
  ASSERT_EQ(state, (TrivialState("foo", 1)));
  runtime->softShutdown();
}

TYPED_TEST(RuntimeTest, sends_initial_message_when_spawning_actor) {
  auto runtime = this->makeRuntime();

  auto actor = runtime->template spawn<TrivialActor>(
      std::make_unique<TrivialState>("foo"),
      test::message::TrivialMessage("bar"));

  this->scheduler->stop();
  auto state = runtime->template getActorStateByID<TrivialActor>(actor);
  ASSERT_EQ(state, (TrivialState("foobar", 1)));
  runtime->softShutdown();
}

TYPED_TEST(RuntimeTest, gives_all_existing_actor_ids) {
  auto runtime = this->makeRuntime();

  ASSERT_TRUE(runtime->getActorIDs().empty());

  auto actor_foo = runtime->template spawn<TrivialActor>(
      std::make_unique<TrivialState>("foo"), test::message::TrivialStart());
  auto actor_bar = runtime->template spawn<TrivialActor>(
      std::make_unique<TrivialState>("bar"), test::message::TrivialStart());

  this->scheduler->stop();
  auto allActorIDs = runtime->getActorIDs();
  ASSERT_EQ(allActorIDs.size(), 2);
  ASSERT_EQ(
      (std::unordered_set<ActorID>(allActorIDs.begin(), allActorIDs.end())),
      (std::unordered_set<ActorID>{actor_foo.id, actor_bar.id}));
  runtime->softShutdown();
}

TYPED_TEST(RuntimeTest, sends_message_to_an_actor) {
  auto runtime = this->makeRuntime();
  auto actor = runtime->template spawn<TrivialActor>(
      std::make_unique<TrivialState>("foo"), test::message::TrivialStart{});

  runtime->dispatch(
      actor, actor,
      TrivialActor::Message{test::message::TrivialMessage("baz")});

  this->scheduler->stop();
  auto state = runtime->template getActorStateByID<TrivialActor>(actor);
  ASSERT_EQ(state, (TrivialState("foobaz", 2)));
  runtime->softShutdown();
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
    RuntimeTest,
    actor_receiving_wrong_message_type_sends_back_unknown_error_message) {
  auto runtime = this->makeRuntime();
  auto actor = runtime->template spawn<TrivialActor>(
      std::make_unique<TrivialState>("foo"), test::message::TrivialStart{});

  runtime->dispatch(actor, actor, SomeMessages{SomeMessage{}});

  this->scheduler->stop();
  ASSERT_EQ(
      runtime->template getActorStateByID<TrivialActor>(actor),
      (TrivialState(fmt::format("sent unknown message to {}", actor), 2)));
  runtime->softShutdown();
}

TYPED_TEST(
    RuntimeTest,
    actor_receives_actor_not_found_message_after_trying_to_send_message_to_non_existent_actor) {
  auto runtime = this->makeRuntime();
  auto actor = runtime->template spawn<TrivialActor>(
      std::make_unique<TrivialState>("foo"), test::message::TrivialStart{});

  auto unknown_actor = this->makePid(ActorID{999});
  runtime->dispatch(
      actor, unknown_actor,
      TrivialActor::Message{test::message::TrivialMessage{"baz"}});

  this->scheduler->stop();
  ASSERT_EQ(
      runtime->template getActorStateByID<TrivialActor>(actor),
      (TrivialState(fmt::format("receiving actor {} not found", unknown_actor),
                    2)));
  runtime->softShutdown();
}

TYPED_TEST(RuntimeTest, ping_pong_game) {
  auto runtime = this->makeRuntime();

  auto pong_actor = runtime->template spawn<pong_actor::Actor>(
      std::make_unique<pong_actor::PongState>(), pong_actor::message::Start{});
  auto ping_actor =
      runtime
          ->template spawn<ping_actor::Actor<typename TestFixture::ActorPID>>(
              std::make_unique<ping_actor::PingState>(),
              ping_actor::message::Start<typename TestFixture::ActorPID>{
                  .pongActor = pong_actor});

  this->scheduler->stop();
  auto ping_actor_state = runtime->template getActorStateByID<
      ping_actor::Actor<typename TestFixture::ActorPID>>(ping_actor);
  ASSERT_EQ(ping_actor_state,
            (ping_actor::PingState{.called = 2, .message = "hello world"}));
  auto pong_actor_state =
      runtime->template getActorStateByID<pong_actor::Actor>(pong_actor);
  ASSERT_EQ(pong_actor_state, (pong_actor::PongState{.called = 2}));
  runtime->softShutdown();
}

TYPED_TEST(RuntimeTest, spawn_game) {
  auto runtime = this->makeRuntime();

  auto spawn_actor = runtime->template spawn<SpawnActor>(
      std::make_unique<SpawnState>(), test::message::SpawnStartMessage{});

  runtime->dispatch(spawn_actor, spawn_actor,
                    SpawnActor::Message{test::message::SpawnMessage{"baz"}});

  this->scheduler->stop();
  ASSERT_EQ(runtime->getActorIDs().size(), 2);
  ASSERT_EQ(runtime->template getActorStateByID<SpawnActor>(spawn_actor),
            (SpawnState{.called = 2, .state = "baz"}));
  runtime->softShutdown();
}

TYPED_TEST(RuntimeTest, finishes_actor_when_actor_says_so) {
  auto runtime = this->makeRuntime();

  auto finishing_actor = runtime->template spawn<FinishingActor>(
      std::make_unique<FinishingState>(), test::message::FinishingStart{});

  runtime->dispatch(finishing_actor, finishing_actor,
                    FinishingActor::Message{test::message::FinishingFinish{}});

  this->scheduler->stop();
  ASSERT_FALSE(runtime->actors.find(finishing_actor.id).has_value());
  runtime->softShutdown();
}

TYPED_TEST(RuntimeTest, finished_actor_automatically_removes_itself) {
  auto runtime = this->makeRuntime();
  auto finishing_actor = runtime->template spawn<FinishingActor>(
      std::make_unique<FinishingState>(), test::message::FinishingStart{});

  runtime->dispatch(finishing_actor, finishing_actor,
                    FinishingActor::Message{test::message::FinishingFinish{}});
  waitForAllMessagesToBeProcessed(runtime, *this->scheduler);

  this->scheduler->stop();
  ASSERT_EQ(runtime->actors.size(), 0);
}

TYPED_TEST(RuntimeTest, finished_actors_automatically_remove_themselves) {
  auto runtime = this->makeRuntime();

  auto actor_to_be_finished = runtime->template spawn<FinishingActor>(
      std::make_unique<FinishingState>(), test::message::FinishingStart{});
  runtime->template spawn<FinishingActor>(std::make_unique<FinishingState>(),
                                          test::message::FinishingStart{});
  runtime->template spawn<FinishingActor>(std::make_unique<FinishingState>(),
                                          test::message::FinishingStart{});
  auto another_actor_to_be_finished = runtime->template spawn<FinishingActor>(
      std::make_unique<FinishingState>(), test::message::FinishingStart{});
  runtime->template spawn<FinishingActor>(std::make_unique<FinishingState>(),
                                          test::message::FinishingStart{});

  runtime->dispatch(actor_to_be_finished, actor_to_be_finished,
                    FinishingActor::Message{test::message::FinishingFinish{}});
  runtime->dispatch(another_actor_to_be_finished, another_actor_to_be_finished,
                    FinishingActor::Message{test::message::FinishingFinish{}});
  waitForAllMessagesToBeProcessed(runtime, *this->scheduler);

  this->scheduler->stop();
  ASSERT_EQ(runtime->actors.size(), 3);
  auto remaining_actor_ids = runtime->getActorIDs();
  std::unordered_set<ActorID> actor_ids(remaining_actor_ids.begin(),
                                        remaining_actor_ids.end());
  ASSERT_FALSE(actor_ids.contains(actor_to_be_finished.id));
  ASSERT_FALSE(actor_ids.contains(another_actor_to_be_finished.id));
  runtime->softShutdown();
  ASSERT_EQ(runtime->actors.size(), 0);
}

TYPED_TEST(RuntimeTest,
           finishes_and_garbage_collects_all_actors_when_shutting_down) {
  auto runtime = this->makeRuntime();
  runtime->template spawn<TrivialActor>(std::make_unique<TrivialState>(),
                                        test::message::TrivialStart{});
  runtime->template spawn<TrivialActor>(std::make_unique<TrivialState>(),
                                        test::message::TrivialStart{});
  runtime->template spawn<TrivialActor>(std::make_unique<TrivialState>(),
                                        test::message::TrivialStart{});
  runtime->template spawn<TrivialActor>(std::make_unique<TrivialState>(),
                                        test::message::TrivialStart{});
  runtime->template spawn<TrivialActor>(std::make_unique<TrivialState>(),
                                        test::message::TrivialStart{});
  ASSERT_EQ(runtime->actors.size(), 5);
  waitForAllMessagesToBeProcessed(runtime, *this->scheduler);
  this->scheduler->stop();
  runtime->softShutdown();
  ASSERT_EQ(runtime->actors.size(), 0);
}

TYPED_TEST(RuntimeTest, sends_down_message_to_monitoring_actors) {
  auto runtime = this->makeRuntime();
  auto monitor1 = runtime->template spawn<MonitoringActor>(
      std::make_unique<MonitoringState>());
  auto monitor2 = runtime->template spawn<MonitoringActor>(
      std::make_unique<MonitoringState>());
  auto monitor3 = runtime->template spawn<MonitoringActor>(
      std::make_unique<MonitoringState>());

  auto monitored1 = runtime->template spawn<FinishingActor>(
      std::make_unique<FinishingState>());
  auto monitored2 = runtime->template spawn<FinishingActor>(
      std::make_unique<FinishingState>());
  auto monitored3 = runtime->template spawn<FinishingActor>(
      std::make_unique<FinishingState>());

  ASSERT_EQ(runtime->actors.size(), 6);

  runtime->monitorActor(monitor1, monitored1);
  runtime->monitorActor(monitor1, monitored2);
  runtime->monitorActor(monitor2, monitored2);
  runtime->monitorActor(monitor3, monitored2);
  runtime->monitorActor(monitor3, monitored3);

  runtime->dispatch(monitored2, monitored2,
                    FinishingActor::Message{test::message::FinishingFinish{}});
  waitForAllMessagesToBeProcessed(runtime, *this->scheduler);

  EXPECT_EQ(runtime->actors.size(), 5);
  EXPECT_EQ(
      (MonitoringState{.deadActors = {{monitored2.id, ExitReason::kFinished}}}),
      *runtime->template getActorStateByID<MonitoringActor>(monitor1));
  EXPECT_EQ(
      (MonitoringState{.deadActors = {{monitored2.id, ExitReason::kFinished}}}),
      *runtime->template getActorStateByID<MonitoringActor>(monitor2));
  EXPECT_EQ(
      (MonitoringState{.deadActors = {{monitored2.id, ExitReason::kFinished}}}),
      *runtime->template getActorStateByID<MonitoringActor>(monitor3));

  runtime->dispatch(monitored1, monitored1,
                    FinishingActor::Message{
                        test::message::FinishingFinish{ExitReason::kShutdown}});
  runtime->dispatch(monitored3, monitored3,
                    FinishingActor::Message{
                        test::message::FinishingFinish{ExitReason::kShutdown}});
  waitForAllMessagesToBeProcessed(runtime, *this->scheduler);

  EXPECT_EQ(
      (MonitoringState{.deadActors = {{monitored2.id, ExitReason::kFinished},
                                      {monitored1.id, ExitReason::kShutdown}}}),
      *runtime->template getActorStateByID<MonitoringActor>(monitor1));
  EXPECT_EQ(
      (MonitoringState{.deadActors = {{monitored2.id, ExitReason::kFinished}}}),
      *runtime->template getActorStateByID<MonitoringActor>(monitor2));
  EXPECT_EQ(
      (MonitoringState{.deadActors = {{monitored2.id, ExitReason::kFinished},
                                      {monitored3.id, ExitReason::kShutdown}}}),
      *runtime->template getActorStateByID<MonitoringActor>(monitor3));

  this->scheduler->stop();
  runtime->softShutdown();
  ASSERT_EQ(runtime->actors.size(), 0);
}

TYPED_TEST(
    RuntimeTest,
    trying_to_monitor_an_already_terminated_actor_immediately_sends_ActorDown_message) {
  auto runtime = this->makeRuntime();
  auto monitor = runtime->template spawn<MonitoringActor>(
      std::make_unique<MonitoringState>());

  runtime->monitorActor(monitor, this->makePid(ActorID{999}));

  waitForAllMessagesToBeProcessed(runtime, *this->scheduler);
  EXPECT_EQ(
      (MonitoringState{.deadActors = {{ActorID{999}, ExitReason::kUnknown}}}),
      *runtime->template getActorStateByID<MonitoringActor>(monitor));

  this->scheduler->stop();
  runtime->softShutdown();
}

TYPED_TEST(
    RuntimeTest,
    trying_to_dispatching_a_message_to_a_non_existing_actor_does_not_crash_if_sender_no_longer_exists) {
  auto runtime = this->makeRuntime();

  runtime->monitorActor(this->makePid(ActorID{998}),
                        this->makePid(ActorID{999}));

  this->scheduler->stop();
  runtime->softShutdown();
}

template<typename TRuntime>
struct RuntimeStressTest : public testing::Test {
  using Runtime = TRuntime;

  auto makePid(ActorID id) { return RuntimeFactor<TRuntime>::makePid(id); }
};

using Runtimes = ::testing::Types<DistributedRuntime, LocalRuntime>;
TYPED_TEST_SUITE(RuntimeStressTest, Runtimes);

TYPED_TEST(RuntimeStressTest, sends_messages_between_lots_of_actors) {
  auto scheduler = std::make_shared<ThreadPoolScheduler>();
  auto runtime =
      RuntimeFactor<typename TestFixture::Runtime>::create(scheduler);

  scheduler->start(128);
  size_t actor_count = 128;

  for (size_t i = 0; i < actor_count; i++) {
    runtime->template spawn<TrivialActor>(std::make_unique<TrivialState>(),
                                          test::message::TrivialStart{});
  }

  // send from actor i to actor i+1 a message with content i
  for (size_t i = 1; i < actor_count; i++) {
    runtime->dispatch(this->makePid(ActorID{(i + 1) % actor_count}),
                      this->makePid(ActorID{i}),
                      TrivialActor::Message{
                          test::message::TrivialMessage{std::to_string(i)}});
  }
  // send from actor actor_count to actor 1 (jump over special actor id 0)
  runtime->dispatch(
      this->makePid(ActorID{1}), this->makePid(ActorID{actor_count}),
      TrivialActor::Message{
          test::message::TrivialMessage{std::to_string(actor_count)}});

  waitForAllMessagesToBeProcessed(runtime, *scheduler);

  scheduler->stop();
  ASSERT_EQ(runtime->actors.size(), actor_count);
  for (size_t i = 1; i < actor_count; i++) {
    ASSERT_EQ(runtime->template getActorStateByID<TrivialActor>(ActorID{i}),
              (TrivialState(std::to_string(i), 2)));
  }
  ASSERT_EQ(
      runtime->template getActorStateByID<TrivialActor>(ActorID{actor_count}),
      (TrivialState(std::to_string(actor_count), 2)));
  runtime->softShutdown();
}
