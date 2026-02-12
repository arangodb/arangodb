////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Activities/registry.h"
#include "Async/async.h"
#include "Containers/Concurrent/thread.h"
#include "Activities/activity.h"
#include "Activities/activity_registry_variable.h"
#include "Inspection/JsonPrintInspector.h"
#include <gtest/gtest.h>

#include <coroutine>
#include <thread>
#include <deque>

#include "Async/WaitTypes.h"

using namespace arangodb;
using namespace arangodb::activities;

namespace {
auto promise_count_in_registry() -> uint {
  uint promise_count = 0;
  arangodb::async_registry::get_thread_registry().for_node(
      [&](arangodb::async_registry::PromiseSnapshot promise) {
        promise_count++;
      });
  return promise_count;
}
}  // namespace

template<typename WaitType>
struct ActivitiesAsyncTest : ::testing::Test {
  void SetUp() override {}

  void TearDown() override {
    arangodb::async_registry::get_thread_registry().garbage_collect();
    wait.stop();
    wait2.stop();
    EXPECT_EQ(promise_count_in_registry(), 0);
    EXPECT_TRUE(std::holds_alternative<
                arangodb::containers::SharedPtr<arangodb::basics::ThreadInfo>>(
        *arangodb::async_registry::get_current_coroutine()));
  }

  auto activityName(std::string n) -> std::string {
    return std::string{::testing::UnitTest::GetInstance()
                           ->current_test_info()
                           ->name()} +
           n;
  }
  auto makeActivity(std::string name, Metadata md) -> Activity {
    return Activity(this->activityName(name), md);
  }
  WaitType wait;
  WaitType wait2;
};

using MyTypes = ::testing::Types<async_tests::NoWait, async_tests::WaitSlot,
                                 async_tests::ConcurrentNoWait>;
TYPED_TEST_SUITE(ActivitiesAsyncTest, MyTypes);

TYPED_TEST(ActivitiesAsyncTest, root_activity_persists) {
  ASSERT_EQ((activities::Registry::currentlyExecutingActivity()), ActivityRoot);

  auto coro = [&]() -> async<void> {
    EXPECT_EQ((activities::Registry::currentlyExecutingActivity()),
              ActivityRoot);

    auto coro_activity = this->makeActivity("CoroActivity", {});
    auto guard = activities::Registry::ScopedCurrentlyExecutingActivity(
        coro_activity.id());

    EXPECT_EQ((activities::Registry::currentlyExecutingActivity()),
              coro_activity.id());

    co_await this->wait;

    EXPECT_EQ((activities::Registry::currentlyExecutingActivity()),
              coro_activity.id());

    co_return;
  }();

  auto outer_activity = this->makeActivity("OuterActivity", {});
  auto guard = activities::Registry::ScopedCurrentlyExecutingActivity(
      outer_activity.id());

  EXPECT_EQ(activities::Registry::currentlyExecutingActivity(),
            outer_activity.id());

  this->wait.resume();

  EXPECT_EQ(activities::Registry::currentlyExecutingActivity(),
            outer_activity.id());

  auto awaitable = std::move(coro).operator co_await();
  this->wait.await();
  awaitable.await_resume();

  EXPECT_EQ(activities::Registry::currentlyExecutingActivity(),
            outer_activity.id());
}

TYPED_TEST(ActivitiesAsyncTest, current_activity_persists_parenting_works) {
  ASSERT_EQ((activities::Registry::currentlyExecutingActivity()), ActivityRoot);

  auto outer_activity = this->makeActivity("OuterActivity", {});
  auto guard = activities::Registry::ScopedCurrentlyExecutingActivity(
      outer_activity.id());

  ASSERT_EQ((activities::Registry::currentlyExecutingActivity()),
            outer_activity.id());

  auto coro = [&]() -> async<void> {
    EXPECT_EQ((activities::Registry::currentlyExecutingActivity()),
              outer_activity.id());

    auto coro_activity = this->makeActivity("TestActivity", {});

    EXPECT_EQ(coro_activity.parentId(), outer_activity.id());

    auto guard = activities::Registry::ScopedCurrentlyExecutingActivity(
        coro_activity.id());

    EXPECT_EQ((activities::Registry::currentlyExecutingActivity()),
              coro_activity.id());

    co_await this->wait;

    EXPECT_EQ((activities::Registry::currentlyExecutingActivity()),
              coro_activity.id());

    co_return;
  }();

  auto next_outer_activity = this->makeActivity("OuterActivity", {});
  auto next_guard = activities::Registry::ScopedCurrentlyExecutingActivity(
      next_outer_activity.id());

  EXPECT_EQ(activities::Registry::currentlyExecutingActivity(),
            next_outer_activity.id());

  this->wait.resume();

  EXPECT_EQ(activities::Registry::currentlyExecutingActivity(),
            next_outer_activity.id());

  auto awaitable = std::move(coro).operator co_await();
  this->wait.await();
  awaitable.await_resume();

  EXPECT_EQ(activities::Registry::currentlyExecutingActivity(),
            next_outer_activity.id());
}

TYPED_TEST(ActivitiesAsyncTest, current_activity_persists_multiple_coros) {
  auto coro = [&](auto& wait) -> async<void> {
    auto coro_activity = activities::Activity("TestActivity", {});
    auto guard = activities::Registry::ScopedCurrentlyExecutingActivity(
        coro_activity.id());

    EXPECT_EQ((activities::Registry::currentlyExecutingActivity()),
              coro_activity.id());

    co_await wait;

    // currentlyExecutingActivity survives suspend/resume
    EXPECT_EQ((activities::Registry::currentlyExecutingActivity()),
              coro_activity.id());
    co_return;
  };

  auto coro1 = coro(this->wait);
  auto coro2 = coro(this->wait2);

  auto outer_activity = activities::Activity("OuterTestActivity", {});
  auto guard = activities::Registry::ScopedCurrentlyExecutingActivity(
      outer_activity.id());
  EXPECT_EQ(activities::Registry::currentlyExecutingActivity(),
            outer_activity.id());

  this->wait2.resume();
  this->wait.resume();

  EXPECT_EQ(activities::Registry::currentlyExecutingActivity(),
            outer_activity.id());

  auto awaitable = std::move(coro1).operator co_await();
  this->wait.await();
  awaitable.await_resume();

  auto awaitable2 = std::move(coro2).operator co_await();
  this->wait2.await();
  awaitable2.await_resume();

  EXPECT_EQ(activities::Registry::currentlyExecutingActivity(),
            outer_activity.id());
}

TYPED_TEST(ActivitiesAsyncTest,
           current_activity_persists_multiple_suspension_points) {
  auto coro = [&]() -> async<void> {
    auto coro_activity = this->makeActivity("TestActivity", {});
    auto guard = activities::Registry::ScopedCurrentlyExecutingActivity(
        coro_activity.id());

    EXPECT_EQ((activities::Registry::currentlyExecutingActivity()),
              coro_activity.id());

    co_await this->wait;
    EXPECT_EQ((activities::Registry::currentlyExecutingActivity()),
              coro_activity.id());

    co_await this->wait;
    EXPECT_EQ((activities::Registry::currentlyExecutingActivity()),
              coro_activity.id());

    co_return;
  }();

  auto outer_activity = this->makeActivity("OuterActivity", {});
  auto guard = activities::Registry::ScopedCurrentlyExecutingActivity(
      outer_activity.id());
  EXPECT_EQ(activities::Registry::currentlyExecutingActivity(),
            outer_activity.id());

  this->wait.resume();

  EXPECT_EQ(activities::Registry::currentlyExecutingActivity(),
            outer_activity.id());

  this->wait.await();

  EXPECT_EQ(activities::Registry::currentlyExecutingActivity(),
            outer_activity.id());
}

TYPED_TEST(ActivitiesAsyncTest, current_activity_persists_nested_coroutines) {
  auto coro_inner = [&]() -> async<void> {
    auto coro_activity = this->makeActivity("InnerCoroActivity", {});
    auto guard = activities::Registry::ScopedCurrentlyExecutingActivity(
        coro_activity.id());
    co_await this->wait;
    EXPECT_EQ((activities::Registry::currentlyExecutingActivity()),
              coro_activity.id());
  };

  auto coro = [&]() -> async<void> {
    auto coro_activity = this->makeActivity("TestActivity", {});
    auto guard = activities::Registry::ScopedCurrentlyExecutingActivity(
        coro_activity.id());

    co_await coro_inner();

    EXPECT_EQ((activities::Registry::currentlyExecutingActivity()),
              coro_activity.id());

    co_return;
  }();

  auto outer_activity = this->makeActivity("OuterTestActivity", {});
  auto guard = activities::Registry::ScopedCurrentlyExecutingActivity(
      outer_activity.id());
  EXPECT_EQ(activities::Registry::currentlyExecutingActivity(),
            outer_activity.id());

  this->wait.resume();

  EXPECT_EQ(activities::Registry::currentlyExecutingActivity(),
            outer_activity.id());

  this->wait.await();

  EXPECT_EQ(activities::Registry::currentlyExecutingActivity(),
            outer_activity.id());
}

TYPED_TEST(ActivitiesAsyncTest, current_activity_correct_exception) {
  auto a = [&]() -> async<void> {
    auto coro_activity = this->makeActivity("ATestActivity", {});
    auto guard = activities::Registry::ScopedCurrentlyExecutingActivity(
        coro_activity.id());

    co_await this->wait;
    throw std::runtime_error("TEST!");
  }();

  auto b = [&]() -> async<void> {
    auto coro_activity = this->makeActivity("BTestActivity", {});
    auto guard = activities::Registry::ScopedCurrentlyExecutingActivity(
        coro_activity.id());

    try {
      co_await std::move(a);
      EXPECT_EQ(activities::Registry::currentlyExecutingActivity(),
                coro_activity.id());
      TRI_ASSERT(false);
      co_return;
    } catch (std::runtime_error const&) {
      // catching an exception should not bypass activity settings
      EXPECT_EQ(activities::Registry::currentlyExecutingActivity(),
                coro_activity.id());
      co_return;
    }
  }();

  this->wait.resume();
  EXPECT_TRUE(b.valid());
  EXPECT_FALSE(a.valid());
  auto awaitable = std::move(b).operator co_await();
  this->wait.await();
  EXPECT_TRUE(awaitable.await_ready());
  awaitable.await_resume();
}
