////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
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

#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <random>
#include <thread>

#include "GeneralServer/RequestLane.h"
#include "Scheduler/SchedulerFeature.h"
#include "Scheduler/SupervisedScheduler.h"
#include "Scheduler/AcceptanceQueue/AcceptanceQueueImpl.h"
#include "Mocks/Servers.h"
#include "Metrics/MetricsFeature.h"

using namespace arangodb;

struct AcceptanceQueueFast {
  static constexpr auto limit = 1024 * 64;

  explicit AcceptanceQueueFast(
      tests::mocks::MockRestServer& mockApplicationServer,
      unsigned const numThreads)
      : metricsFeature(std::make_shared<arangodb::metrics::MetricsFeature>(
            mockApplicationServer.server(),
            LazyApplicationFeatureReference<QueryRegistryFeature>(nullptr),
            LazyApplicationFeatureReference<StatisticsFeature>(nullptr),
            LazyApplicationFeatureReference<EngineSelectorFeature>(nullptr),
            LazyApplicationFeatureReference<metrics::ClusterMetricsFeature>(
                nullptr),
            LazyApplicationFeatureReference<ClusterFeature>(nullptr))),
        schedulerMetrics(std::make_shared<SchedulerMetrics>(*metricsFeature)),
        antiOverwhelm(std::make_shared<LowPrioAntiOverwhelm>(
            mockApplicationServer.server(), limit, schedulerMetrics)),
        scheduler(mockApplicationServer.server(), numThreads, numThreads, limit,
                  limit, limit, limit, schedulerMetrics, antiOverwhelm),
        acceptanceQueue(limit, limit, limit, limit, 0.0, &scheduler,
                        schedulerMetrics, antiOverwhelm) {
    scheduler.start();
    acceptanceQueue.start();
    // SchedulerFeature::ACCEPTANCE_QUEUE = &acceptanceQueue;
  }

  ~AcceptanceQueueFast() { shutdown(); }

  void shutdown() {
    scheduler.shutdown();
    acceptanceQueue.shutdown();
  }
  // we create multiple schedulers, so each one needs its own metrics feature to
  // register its metrics
  std::shared_ptr<arangodb::metrics::MetricsFeature> metricsFeature;
  std::shared_ptr<SchedulerMetrics> schedulerMetrics;
  std::shared_ptr<LowPrioAntiOverwhelm> antiOverwhelm;
  SupervisedScheduler scheduler;
  AcceptanceQueueImpl acceptanceQueue;

  template<std::invocable Fn>
  void push(Fn&& fn) {
    auto const wasQueued = acceptanceQueue.tryBoundedQueue(
        RequestLane::CLIENT_FAST, std::forward<Fn>(fn));
    GTEST_ASSERT_TRUE(wasQueued);
  }
};

struct AcceptanceQueueSlow {
  static constexpr auto limit = 1024 * 64;

  explicit AcceptanceQueueSlow(
      tests::mocks::MockRestServer& mockApplicationServer,
      unsigned const numThreads)
      : metricsFeature(std::make_shared<arangodb::metrics::MetricsFeature>(
            mockApplicationServer.server(),
            LazyApplicationFeatureReference<QueryRegistryFeature>(nullptr),
            LazyApplicationFeatureReference<StatisticsFeature>(nullptr),
            LazyApplicationFeatureReference<EngineSelectorFeature>(nullptr),
            LazyApplicationFeatureReference<metrics::ClusterMetricsFeature>(
                nullptr),
            LazyApplicationFeatureReference<ClusterFeature>(nullptr))),
        schedulerMetrics(std::make_shared<SchedulerMetrics>(*metricsFeature)),
        antiOverwhelm(std::make_shared<LowPrioAntiOverwhelm>(
            mockApplicationServer.server(), limit, schedulerMetrics)),
        scheduler(mockApplicationServer.server(), numThreads, numThreads, limit,
                  limit, limit, limit, schedulerMetrics, antiOverwhelm),
        acceptanceQueue(limit, limit, limit, limit, 0.0, &scheduler,
                        schedulerMetrics, antiOverwhelm) {
    scheduler.start();
    acceptanceQueue.start();
    // SchedulerFeature::ACCEPTANCE_QUEUE = &acceptanceQueue;
  }

  ~AcceptanceQueueSlow() { shutdown(); }

  void shutdown() {
    scheduler.shutdown();
    acceptanceQueue.shutdown();
  }

  // we create multiple schedulers, so each one needs its own metrics feature to
  // register its metrics
  std::shared_ptr<arangodb::metrics::MetricsFeature> metricsFeature;
  std::shared_ptr<SchedulerMetrics> schedulerMetrics;
  std::shared_ptr<LowPrioAntiOverwhelm> antiOverwhelm;
  SupervisedScheduler scheduler;
  AcceptanceQueueImpl acceptanceQueue;

  template<std::invocable Fn>
  void push(Fn&& fn) {
    auto const wasQueued = acceptanceQueue.tryBoundedQueue(
        RequestLane::CLIENT_SLOW, std::forward<Fn>(fn));
    GTEST_ASSERT_TRUE(wasQueued);
  }
};

template<class T>
struct PoolBuilder {
  T makePool(const char* name, unsigned numThreads) {
    return T{name, numThreads};
  }
};

template<>
struct PoolBuilder<AcceptanceQueueSlow> {
  tests::mocks::MockRestServer mockApplicationServer;

  AcceptanceQueueSlow makePool(const char*, unsigned numThreads) {
    return AcceptanceQueueSlow{mockApplicationServer, numThreads};
  }
};

template<>
struct PoolBuilder<AcceptanceQueueFast> {
  tests::mocks::MockRestServer mockApplicationServer;

  AcceptanceQueueFast makePool(const char*, unsigned numThreads) {
    return AcceptanceQueueFast{mockApplicationServer, numThreads};
  }
};

template<typename Pool>
class BoundedSchedulerPerfTest : public testing::Test {
  void SetUp() override {
    arangodb::Logger::CLUSTER.setLogLevel(LogLevel::ERR);
    arangodb::Logger::THREADS.setLogLevel(LogLevel::ERR);
  }
};

using PoolTypes = ::testing::Types<AcceptanceQueueSlow, AcceptanceQueueFast>;
TYPED_TEST_SUITE(BoundedSchedulerPerfTest, PoolTypes);

namespace {
template<typename Pool>
struct callable;

template<typename Pool>
callable<Pool> createLambda(std::atomic<std::uint64_t>& cnt, Pool& pool, int x,
                            std::atomic<bool>& stop);

template<typename Pool>
struct callable {
  void operator()() const noexcept {
    if (!stop.load()) {
      cnt.fetch_add(1);
      pool.push(createLambda(cnt, pool, x + 1, stop));
      pool.push(createLambda(cnt, pool, x + 1, stop));

      // simulate some work
      unsigned i = 0;
      static constexpr unsigned workLimit = 2 << 13;
      while (++i < workLimit) {
        // not doing the stop check in every iteration is especially important
        // when running with TSan!
        if (i % 1024 == 0 && stop.load()) {
          break;
        }
      }
    }
  }

  std::atomic<std::uint64_t>& cnt;
  std::atomic<bool>& stop;
  Pool& pool;
  const int x;
};

template<typename Pool>
callable<Pool> createLambda(std::atomic<std::uint64_t>& cnt, Pool& pool, int x,
                            std::atomic<bool>& stop) {
  return callable<Pool>{cnt, stop, pool, x};
}
}  // namespace

enum class WorkSimulation { None, Deterministic, Random };

template<typename Pool>
struct PingPong {
  PingPong(Pool* pool1, Pool* pool2, int id, int ping, std::atomic<bool>& stop,
           std::atomic<std::uint64_t>& counter, WorkSimulation work)
      : pools{pool1, pool2},
        id(id),
        ping(ping),
        stop(stop),
        counter(counter),
        work(work) {}

  void operator()() noexcept {
    if (!stop.load()) {
      if (work != WorkSimulation::None) {
        unsigned i = 0;
        static constexpr unsigned deterministicWorkLimit = 2 << 11;
        static constexpr unsigned randomWorkLimit = 2 << 14;
        unsigned workLimit = work == WorkSimulation::Deterministic
                                 ? deterministicWorkLimit
                                 : std::mt19937_64{static_cast<unsigned long>(
                                       ping + id * 123456789)}() &
                                       (randomWorkLimit - 1);
        while (++i < workLimit) {
          // not doing the stop check in every iteration is especially important
          // when running with TSan!
          if (i % 1024 == 0 && stop.load()) {
            break;
          }
        }
      }

      ++ping;
      pools[ping & 1]->push(
          PingPong(pools[0], pools[1], id, ping, stop, counter, work));
      counter.fetch_add(1);
    }
  }

  Pool* pools[2];
  int id;
  int ping;
  std::atomic<bool>& stop;
  std::atomic<std::uint64_t>& counter;
  WorkSimulation work;
};

template<typename Pool>
std::size_t pingPongTest(unsigned numThreads, unsigned numBalls,
                         WorkSimulation work) {
  std::atomic<bool> stopSignal = false;
  std::atomic<std::uint64_t> counter = 0;

  std::uint64_t durationMs = 0;
  {
    PoolBuilder<Pool> poolBuilder;
    auto pool1 = poolBuilder.makePool("pool1", numThreads);
    auto pool2 = poolBuilder.makePool("pool2", numThreads);

    auto start = std::chrono::steady_clock::now();
    for (unsigned i = 0; i < numBalls; ++i) {
      pool1.push(
          PingPong<Pool>(&pool1, &pool2, i, 0, stopSignal, counter, work));
    }

    std::this_thread::sleep_for(std::chrono::seconds{6});
    stopSignal.store(true);

    durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                     std::chrono::steady_clock::now() - start)
                     .count();

    std::this_thread::sleep_for(std::chrono::milliseconds{100});

    // need to explicitly shut down the pools, otherwise one pool might still
    // try to push to a pool that is already being destroyed
    pool2.shutdown();
    pool1.shutdown();
  }
  auto numOps = counter.load();
  return numOps / durationMs;
}

template<typename Pool>
void runPingPong(WorkSimulation const work) {
  std::array<unsigned, 4> const threads = {5, 13, 41, 67};
  std::array<unsigned, 7> const balls = {1, 4, 8, 16, 64, 128, 256};

  std::cout << "              ";
  for (auto b : balls) {
    std::cout << std::setw(3) << b << " balls  ";
  }
  std::cout << "\n";
  for (auto t : threads) {
    std::cout << std::setw(2) << t << " threads: " << std::flush;
    for (auto b : balls) {
      auto throughput = pingPongTest<Pool>(t, b, work);
      std::cout << std::setw(11) << throughput << std::flush;
    }
    std::cout << " ops/ms" << std::endl;
  }
}

TYPED_TEST(BoundedSchedulerPerfTest, ping_pong_no_work) {
  runPingPong<TypeParam>(WorkSimulation::None);
}

TYPED_TEST(BoundedSchedulerPerfTest, ping_pong_deterministic_work) {
  runPingPong<TypeParam>(WorkSimulation::Deterministic);
}

TYPED_TEST(BoundedSchedulerPerfTest, ping_pong_random_work) {
  runPingPong<TypeParam>(WorkSimulation::Random);
}
