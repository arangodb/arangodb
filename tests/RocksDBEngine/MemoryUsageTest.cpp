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
///
/// @author Jan Steemann
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBEngine/RocksDBMethodsMemoryTracker.h"
#include "Basics/GlobalResourceMonitor.h"
#include "Basics/ResourceUsage.h"
#include "Metrics/Gauge.h"

#include "gtest/gtest.h"

#include <memory>

using namespace arangodb;

struct MethodsMemoryUsageTest : public ::testing::Test {
  std::unique_ptr<RocksDBMethodsMemoryTracker> tracker;
};

TEST_F(MethodsMemoryUsageTest, test_standalone) {
  tracker = std::make_unique<RocksDBMethodsMemoryTracker>(
      nullptr, /*metric*/ nullptr, /*granularity*/ 1);

  ASSERT_EQ(0, tracker->memoryUsage());

  tracker->increaseMemoryUsage(1);
  ASSERT_EQ(1, tracker->memoryUsage());

  for (size_t i = 0; i < 1024; ++i) {
    ASSERT_EQ(1 + i * 10, tracker->memoryUsage());
    tracker->increaseMemoryUsage(10);
  }

  for (size_t i = 0; i < 1024; ++i) {
    ASSERT_EQ(1 + (1024 * 10) - i * 10, tracker->memoryUsage());
    tracker->decreaseMemoryUsage(10);
  }

  tracker->increaseMemoryUsage(10'000'000);
  ASSERT_EQ(10'000'001, tracker->memoryUsage());

  tracker->reset();
  ASSERT_EQ(0, tracker->memoryUsage());
}

TEST_F(MethodsMemoryUsageTest, test_using_metric) {
  metrics::Gauge<std::uint64_t> metric(0, "name", "help", /*labels*/ "");
  tracker = std::make_unique<RocksDBMethodsMemoryTracker>(
      nullptr, /*metric*/ &metric, /*granularity*/ 1);

  ASSERT_EQ(0, tracker->memoryUsage());
  ASSERT_EQ(0, metric.load());

  tracker->increaseMemoryUsage(1);
  ASSERT_EQ(1, tracker->memoryUsage());
  ASSERT_EQ(1, metric.load());

  for (size_t i = 0; i < 1024; ++i) {
    ASSERT_EQ(1 + i * 10, tracker->memoryUsage());
    ASSERT_EQ(1 + i * 10, metric.load());
    tracker->increaseMemoryUsage(10);
  }

  for (size_t i = 0; i < 1024; ++i) {
    ASSERT_EQ(1 + (1024 * 10) - i * 10, tracker->memoryUsage());
    ASSERT_EQ(1 + (1024 * 10) - i * 10, metric.load());
    tracker->decreaseMemoryUsage(10);
  }

  tracker->increaseMemoryUsage(10'000'000);
  ASSERT_EQ(10'000'001, tracker->memoryUsage());
  ASSERT_EQ(10'000'001, metric.load());

  tracker->reset();
  ASSERT_EQ(0, tracker->memoryUsage());
  ASSERT_EQ(0, metric.load());
}

TEST_F(MethodsMemoryUsageTest, test_using_resource_monitor_without_query) {
  GlobalResourceMonitor global;
  ResourceMonitor monitor(global);

  tracker = std::make_unique<RocksDBMethodsMemoryTracker>(nullptr, nullptr,
                                                          /*granularity*/ 1);

  ASSERT_EQ(0, tracker->memoryUsage());
  ASSERT_EQ(0, monitor.current());

  tracker->increaseMemoryUsage(1);
  ASSERT_EQ(1, tracker->memoryUsage());
  ASSERT_EQ(0, monitor.current());

  for (size_t i = 0; i < 1024; ++i) {
    ASSERT_EQ(1 + i * 10, tracker->memoryUsage());
    ASSERT_EQ(0, monitor.current());
    tracker->increaseMemoryUsage(10);
  }

  for (size_t i = 0; i < 1024; ++i) {
    ASSERT_EQ(1 + (1024 * 10) - i * 10, tracker->memoryUsage());
    ASSERT_EQ(0, monitor.current());
    tracker->decreaseMemoryUsage(10);
  }

  tracker->increaseMemoryUsage(10'000'000);
  ASSERT_EQ(10'000'001, tracker->memoryUsage());
  ASSERT_EQ(0, monitor.current());

  tracker->reset();
  ASSERT_EQ(0, tracker->memoryUsage());
  ASSERT_EQ(0, monitor.current());
}

TEST_F(MethodsMemoryUsageTest, test_using_resource_monitor_using_query) {
  GlobalResourceMonitor global;
  auto monitor = std::make_shared<ResourceMonitor>(global);

  tracker = std::make_unique<RocksDBMethodsMemoryTracker>(nullptr, nullptr,
                                                          /*granularity*/ 1);

  ASSERT_EQ(0, tracker->memoryUsage());
  ASSERT_EQ(0, monitor->current());

  tracker->increaseMemoryUsage(10'000'000);
  ASSERT_EQ(10'000'000, tracker->memoryUsage());
  ASSERT_EQ(0, monitor->current());

  tracker->beginQuery(monitor);

  tracker->increaseMemoryUsage(1234);
  ASSERT_EQ(10'001'234, tracker->memoryUsage());
  ASSERT_EQ(1234, monitor->current());

  tracker->increaseMemoryUsage(10'000'000);
  ASSERT_EQ(20'001'234, tracker->memoryUsage());
  ASSERT_EQ(10'001'234, monitor->current());

  tracker->decreaseMemoryUsage(234);
  ASSERT_EQ(20'001'000, tracker->memoryUsage());
  ASSERT_EQ(10'001'000, monitor->current());

  tracker->endQuery();
  ASSERT_EQ(10'000'000, tracker->memoryUsage());
  ASSERT_EQ(0, monitor->current());

  tracker->increaseMemoryUsage(1000);
  ASSERT_EQ(10'001'000, tracker->memoryUsage());
  ASSERT_EQ(0, monitor->current());

  tracker->decreaseMemoryUsage(10'001'000);
  ASSERT_EQ(0, tracker->memoryUsage());
  ASSERT_EQ(0, monitor->current());
}

TEST_F(MethodsMemoryUsageTest, test_granularity) {
  GlobalResourceMonitor global;
  auto monitor = std::make_shared<ResourceMonitor>(global);

  tracker =
      std::make_unique<RocksDBMethodsMemoryTracker>(nullptr, nullptr, 1000);

  ASSERT_EQ(0, tracker->memoryUsage());
  ASSERT_EQ(0, monitor->current());

  tracker->beginQuery(monitor);

  tracker->increaseMemoryUsage(1);
  ASSERT_EQ(1, tracker->memoryUsage());
  ASSERT_EQ(0, monitor->current());

  for (size_t i = 0; i < 998; ++i) {
    ASSERT_EQ(1 + i, tracker->memoryUsage());
    ASSERT_EQ(0, monitor->current());
    tracker->increaseMemoryUsage(1);
  }

  tracker->increaseMemoryUsage(1);
  ASSERT_EQ(1000, tracker->memoryUsage());
  ASSERT_EQ(1000, monitor->current());

  for (size_t i = 0; i < 999; ++i) {
    ASSERT_EQ(1000 + i, tracker->memoryUsage());
    ASSERT_EQ(1000, monitor->current());
    tracker->increaseMemoryUsage(1);
  }

  tracker->increaseMemoryUsage(1);
  ASSERT_EQ(2000, tracker->memoryUsage());
  ASSERT_EQ(2000, monitor->current());

  tracker->endQuery();
}

TEST_F(MethodsMemoryUsageTest, test_using_metric_and_resource_monitor) {
  metrics::Gauge<std::uint64_t> metric(0, "name", "help", /*labels*/ "");
  GlobalResourceMonitor global;
  auto monitor = std::make_shared<ResourceMonitor>(global);
  monitor->increaseMemoryUsage(75);

  tracker = std::make_unique<RocksDBMethodsMemoryTracker>(nullptr, &metric,
                                                          /*granularity*/ 1);

  ASSERT_EQ(0, tracker->memoryUsage());
  ASSERT_EQ(75, monitor->current());
  ASSERT_EQ(0, metric.load());

  tracker->increaseMemoryUsage(10'000'000);
  ASSERT_EQ(10'000'000, tracker->memoryUsage());
  ASSERT_EQ(75, monitor->current());
  ASSERT_EQ(10'000'000, metric.load());

  tracker->beginQuery(monitor);

  tracker->increaseMemoryUsage(1234);
  ASSERT_EQ(10'001'234, tracker->memoryUsage());
  ASSERT_EQ(10'001'234, metric.load());
  ASSERT_EQ(1234 + 75, monitor->current());

  tracker->increaseMemoryUsage(10'000'000);
  ASSERT_EQ(20'001'234, tracker->memoryUsage());
  ASSERT_EQ(20'001'234, metric.load());
  ASSERT_EQ(10'001'234 + 75, monitor->current());

  tracker->decreaseMemoryUsage(234);
  ASSERT_EQ(20'001'000, tracker->memoryUsage());
  ASSERT_EQ(20'001'000, metric.load());
  ASSERT_EQ(10'001'000 + 75, monitor->current());

  tracker->endQuery();
  ASSERT_EQ(10'000'000, tracker->memoryUsage());
  ASSERT_EQ(10'000'000, metric.load());
  ASSERT_EQ(75, monitor->current());

  tracker->increaseMemoryUsage(1000);
  ASSERT_EQ(10'001'000, tracker->memoryUsage());
  ASSERT_EQ(10'001'000, metric.load());
  ASSERT_EQ(75, monitor->current());

  tracker->decreaseMemoryUsage(10'001'000);
  ASSERT_EQ(0, tracker->memoryUsage());
  ASSERT_EQ(0, metric.load());
  ASSERT_EQ(75, monitor->current());

  monitor->decreaseMemoryUsage(75);
  ASSERT_EQ(0, tracker->memoryUsage());
  ASSERT_EQ(0, metric.load());
  ASSERT_EQ(0, monitor->current());
}
