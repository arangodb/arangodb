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

#include "Activities/activity.h"
#include "Activities/registry.h"
#include "Activities/activity_registry_variable.h"
#include "GeneralServer/RequestLane.h"
#include "Mocks/Servers.h"
#include "Scheduler/ThreadPoolScheduler.h"
#include "gtest/gtest.h"
#include "Metrics/MetricsFeature.h"

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
        scheduler(mockApplicationServer.server(), 4, metrics) {}
  void SetUp() override { scheduler.start(); }

  void TearDown() override {
    arangodb::activities::get_thread_registry().garbage_collect();
  }

  arangodb::tests::mocks::MockRestServer mockApplicationServer;
  std::shared_ptr<arangodb::metrics::MetricsFeature> metricsFeature;
  std::shared_ptr<arangodb::SchedulerMetrics> metrics;
  arangodb::ThreadPoolScheduler scheduler;

  std::function<void()> functionToQueue = []() -> void {
    auto activity = arangodb::activities::Activity("TestActivity", {});
    auto guard =
        arangodb::activities::Registry::ScopedCurrentlyExecutingActivity(
            activity.id());

    EXPECT_EQ(arangodb::activities::Registry::currentlyExecutingActivity(),
              activity.id());
    EXPECT_EQ(activity.parentId(), arangodb::activities::ActivityRoot);
  };
};

TEST_F(ActivitiesSchedulerTest, current_activity_persists) {
  auto outer_activity = arangodb::activities::Activity("OuterActivity", {});
  auto guard = arangodb::activities::Registry::ScopedCurrentlyExecutingActivity(
      outer_activity.id());

  EXPECT_EQ(arangodb::activities::Registry::currentlyExecutingActivity(),
            outer_activity.id());

  scheduler.queue(arangodb::RequestLane::CLIENT_FAST, functionToQueue);

  // TODO: Is there a way to know whether the queued thing ran?
  scheduler.shutdown();
  EXPECT_EQ(arangodb::activities::Registry::currentlyExecutingActivity(),
            outer_activity.id());
}

TEST_F(ActivitiesSchedulerTest, multiple_queues) {
  auto outer_activity = arangodb::activities::Activity("OuterActivity", {});
  auto guard = arangodb::activities::Registry::ScopedCurrentlyExecutingActivity(
      outer_activity.id());

  EXPECT_EQ(arangodb::activities::Registry::currentlyExecutingActivity(),
            outer_activity.id());

  scheduler.queue(arangodb::RequestLane::CLIENT_FAST, functionToQueue);
  scheduler.queue(arangodb::RequestLane::CLIENT_FAST, functionToQueue);
  scheduler.queue(arangodb::RequestLane::CLIENT_FAST, functionToQueue);
  scheduler.queue(arangodb::RequestLane::CLIENT_FAST, functionToQueue);

  // TODO: Is there a way to know whether the queued thing ran?
  scheduler.shutdown();
  EXPECT_EQ(arangodb::activities::Registry::currentlyExecutingActivity(),
            outer_activity.id());
}

TEST_F(ActivitiesSchedulerTest, with_set_current_activity_works) {
  auto outer_activity = arangodb::activities::Activity("OuterActivity", {});
  auto guard = arangodb::activities::Registry::ScopedCurrentlyExecutingActivity(
      outer_activity.id());

  auto new_activity = arangodb::activities::Activity("NewActivity", {});
  auto id_to_expect = new_activity.id();
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
            outer_activity.id());
}
