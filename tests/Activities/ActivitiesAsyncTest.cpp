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

template<typename WaitType>
struct ActivitiesAsyncTest : ::testing::Test {
  void SetUp() override {}

  void TearDown() override {
    arangodb::async_registry::get_thread_registry().garbage_collect();
    wait.stop();
    EXPECT_TRUE(std::holds_alternative<
                arangodb::containers::SharedPtr<arangodb::basics::ThreadInfo>>(
        *arangodb::async_registry::get_current_coroutine()));
  }

  WaitType wait;
};

using MyTypes = ::testing::Types<async_tests::NoWait, async_tests::WaitSlot,
                                 async_tests::ConcurrentNoWait>;
TYPED_TEST_SUITE(ActivitiesAsyncTest, MyTypes);

TYPED_TEST(ActivitiesAsyncTest, root_activity_persists) {
  ASSERT_EQ((activities::Registry::currentlyExecutingActivity()), ActivityRoot);

  auto coro = [&]() -> async<void> {
    EXPECT_EQ((activities::Registry::currentlyExecutingActivity()),
              ActivityRoot);

    auto coro_activity = activities::Activity("TestActivity", {});
    auto guard = activities::Registry::ScopedCurrentlyExecutingActivity(
        coro_activity.id());

    EXPECT_EQ((activities::Registry::currentlyExecutingActivity()),
              coro_activity.id());

    co_await this->wait;

    EXPECT_EQ((activities::Registry::currentlyExecutingActivity()),
              coro_activity.id());

    co_return;
  }();

  auto outer_activity = activities::Activity("OuterTestActivity", {});
  auto guard = activities::Registry::ScopedCurrentlyExecutingActivity(
      outer_activity.id());

  EXPECT_EQ(activities::Registry::currentlyExecutingActivity(),
            outer_activity.id());

  this->wait.resume();

  EXPECT_EQ(activities::Registry::currentlyExecutingActivity(),
            outer_activity.id());

  std::ignore = std::move(coro).operator co_await();
  this->wait.await();
  EXPECT_EQ(activities::Registry::currentlyExecutingActivity(),
            outer_activity.id());
}

TYPED_TEST(ActivitiesAsyncTest, current_activity_persists_parenting_works) {
  ASSERT_EQ((activities::Registry::currentlyExecutingActivity()), ActivityRoot);

  auto outer_activity = activities::Activity("OuterTestActivity", {});
  auto guard = activities::Registry::ScopedCurrentlyExecutingActivity(
      outer_activity.id());

  ASSERT_EQ((activities::Registry::currentlyExecutingActivity()),
            outer_activity.id());

  auto coro = [&]() -> async<void> {
    EXPECT_EQ((activities::Registry::currentlyExecutingActivity()),
              outer_activity.id());

    auto coro_activity = activities::Activity("TestActivity", {});

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

  auto next_outer_activity = activities::Activity("OuterTestActivity", {});
  auto next_guard = activities::Registry::ScopedCurrentlyExecutingActivity(
      next_outer_activity.id());

  EXPECT_EQ(activities::Registry::currentlyExecutingActivity(),
            next_outer_activity.id());

  this->wait.resume();

  EXPECT_EQ(activities::Registry::currentlyExecutingActivity(),
            next_outer_activity.id());

  std::ignore = std::move(coro).operator co_await();
  this->wait.await();
  EXPECT_EQ(activities::Registry::currentlyExecutingActivity(),
            next_outer_activity.id());
}

TYPED_TEST(ActivitiesAsyncTest, current_activity_persists_multiple_coros) {
  auto coro = [&]() -> async<void> {
    auto coro_activity = activities::Activity("TestActivity", {});
    auto guard = activities::Registry::ScopedCurrentlyExecutingActivity(
        coro_activity.id());

    EXPECT_EQ((activities::Registry::currentlyExecutingActivity()),
              coro_activity.id());

    co_await this->wait;

    // currentlyExecutingActivity survives suspend/resume
    EXPECT_EQ((activities::Registry::currentlyExecutingActivity()),
              coro_activity.id());
    co_return;
  };

  auto coro1 = coro();
  auto coro2 = coro();

  auto outer_activity = activities::Activity("OuterTestActivity", {});
  auto guard = activities::Registry::ScopedCurrentlyExecutingActivity(
      outer_activity.id());
  EXPECT_EQ(activities::Registry::currentlyExecutingActivity(),
            outer_activity.id());

  this->wait.resume();

  EXPECT_EQ(activities::Registry::currentlyExecutingActivity(),
            outer_activity.id());

  std::ignore = std::move(coro1).operator co_await();
  std::ignore = std::move(coro2).operator co_await();

  this->wait.await();

  EXPECT_EQ(activities::Registry::currentlyExecutingActivity(),
            outer_activity.id());
}

TYPED_TEST(ActivitiesAsyncTest,
           current_activity_persists_multiple_suspension_points) {
  auto coro = [&]() -> async<void> {
    auto coro_activity = activities::Activity("TestActivity", {});
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

    // currentlyExecutingActivity survives suspend/resume
    EXPECT_EQ((activities::Registry::currentlyExecutingActivity()),
              coro_activity.id());
    co_return;
  }();

  auto outer_activity = activities::Activity("OuterTestActivity", {});
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
    auto coro_activity = activities::Activity("InnerCoroActivity", {});
    auto guard = activities::Registry::ScopedCurrentlyExecutingActivity(
        coro_activity.id());
    co_await this->wait;
  };

  auto coro = [&]() -> async<void> {
    auto coro_activity = activities::Activity("TestActivity", {});
    auto guard = activities::Registry::ScopedCurrentlyExecutingActivity(
        coro_activity.id());

    co_await coro_inner();

    EXPECT_EQ((activities::Registry::currentlyExecutingActivity()),
              coro_activity.id());

    co_return;
  }();

  auto outer_activity = activities::Activity("OuterTestActivity", {});
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
