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

#include "Activities/GenericActivity.h"
#include "Activities/RegistryGlobalVariable.h"
#include "GeneralServer/RequestLane.h"
#include "Mocks/Servers.h"
#include "Scheduler/SupervisedScheduler.h"
#include "gtest/gtest.h"
#include "Metrics/MetricsFeature.h"

using namespace arangodb;

struct ActivitiesSchedulerTest : ::testing::Test {
  ActivitiesSchedulerTest()
      : metricsFeature(std::make_shared<arangodb::metrics::MetricsFeature>(
            mockApplicationServer.server(),
            arangodb::LazyApplicationFeatureReference<
                arangodb::QueryRegistryFeature>(nullptr),
            arangodb::LazyApplicationFeatureReference<
                arangodb::StatisticsFeature>(nullptr),
            arangodb::LazyApplicationFeatureReference<
                arangodb::EngineSelectorFeature>(nullptr),
            arangodb::LazyApplicationFeatureReference<
                arangodb::metrics::ClusterMetricsFeature>(nullptr),
            arangodb::LazyApplicationFeatureReference<arangodb::ClusterFeature>(
                nullptr))),
        metrics(std::make_shared<arangodb::SchedulerMetrics>(*metricsFeature)),
        scheduler(mockApplicationServer.server(), 4, 4, 16, 16, 16, 16, 16,
                  0.33, metrics) {}
  void SetUp() override {
    activityData["TestCase"] =
        ::testing::UnitTest::GetInstance()->current_test_info()->name();
    arangodb::activities::Registry::setCurrentlyExecutingActivity(
        arangodb::activities::Root);
    scheduler.start();
  }

  void TearDown() override { arangodb::activities::registry.garbageCollect(); }

  arangodb::tests::mocks::MockRestServer mockApplicationServer;
  std::shared_ptr<arangodb::metrics::MetricsFeature> metricsFeature;
  std::shared_ptr<arangodb::SchedulerMetrics> metrics;
  SupervisedScheduler scheduler;
  activities::GenericActivityData activityData;
};

TEST_F(ActivitiesSchedulerTest, current_activity_persists) {
  auto outer_activity = arangodb::activities::make<activities::GenericActivity>(
      "TestActivity", this->activityData);
  auto guard = arangodb::activities::Registry::ScopedCurrentlyExecutingActivity(
      outer_activity);

  EXPECT_EQ(arangodb::activities::Registry::currentlyExecutingActivity(),
            outer_activity);

  scheduler.queue(
      arangodb::RequestLane::CLIENT_FAST, [this, &outer_activity]() -> void {
        auto activity = arangodb::activities::make<activities::GenericActivity>(
            "TestActivity", this->activityData);
        auto guard =
            arangodb::activities::Registry::ScopedCurrentlyExecutingActivity(
                activity);

        EXPECT_EQ(arangodb::activities::Registry::currentlyExecutingActivity(),
                  activity);
        EXPECT_EQ(activity->parent(), outer_activity);
      });

  // TODO: Is there a way to know whether the queued thing ran?
  scheduler.shutdown();
  EXPECT_EQ(arangodb::activities::Registry::currentlyExecutingActivity(),
            outer_activity);
}

TEST_F(ActivitiesSchedulerTest, multiple_queues) {
  auto outer_activity = arangodb::activities::make<activities::GenericActivity>(
      "TestActivity", this->activityData);
  auto guard = arangodb::activities::Registry::ScopedCurrentlyExecutingActivity(
      outer_activity);

  EXPECT_EQ(arangodb::activities::Registry::currentlyExecutingActivity(),
            outer_activity);

  auto functionToQueue = [this, &outer_activity]() -> void {
    auto activity = arangodb::activities::make<activities::GenericActivity>(
        "TestActivity", this->activityData);
    auto guard =
        arangodb::activities::Registry::ScopedCurrentlyExecutingActivity(
            activity);

    EXPECT_EQ(arangodb::activities::Registry::currentlyExecutingActivity(),
              activity);
    EXPECT_EQ(activity->parent(), outer_activity);
  };

  scheduler.queue(arangodb::RequestLane::CLIENT_FAST, functionToQueue);
  scheduler.queue(arangodb::RequestLane::CLIENT_FAST, functionToQueue);
  scheduler.queue(arangodb::RequestLane::CLIENT_FAST, functionToQueue);
  scheduler.queue(arangodb::RequestLane::CLIENT_FAST, functionToQueue);

  // TODO: Is there a way to know whether the queued thing ran?
  scheduler.shutdown();
  EXPECT_EQ(arangodb::activities::Registry::currentlyExecutingActivity(),
            outer_activity);
}

TEST_F(ActivitiesSchedulerTest, with_set_current_activity_works) {
  auto outer_activity = arangodb::activities::make<activities::GenericActivity>(
      "TestActivity", this->activityData);
  auto guard = arangodb::activities::Registry::ScopedCurrentlyExecutingActivity(
      outer_activity);

  auto new_activity = arangodb::activities::make<activities::GenericActivity>(
      "TestActivity", this->activityData);
  auto id_to_expect = new_activity;
  // No guard is intentional

  scheduler.queue(
      arangodb::RequestLane::CLIENT_FAST,
      arangodb::activities::withSetCurrentlyExecutingActivity(
          id_to_expect, [&id_to_expect]() {
            EXPECT_EQ(
                arangodb::activities::Registry::currentlyExecutingActivity(),
                id_to_expect);
          }));

  // TODO: Is there a way to know whether the queued thing ran?
  scheduler.shutdown();
  EXPECT_EQ(arangodb::activities::Registry::currentlyExecutingActivity(),
            outer_activity);
}
