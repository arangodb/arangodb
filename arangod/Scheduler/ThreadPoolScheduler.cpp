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

#include <velocypack/Builder.h>
#include <numeric>

#include "Metrics/Gauge.h"
#include "Metrics/Counter.h"

#include "ThreadPoolScheduler.h"

using namespace arangodb;

void ThreadPoolScheduler::toVelocyPack(velocypack::Builder& b) const {
  b.add("scheduler-threads", VPackValue(0));  // numWorkers
  b.add("blocked", VPackValue(0));            // obsolete
  b.add("queued", VPackValue(0));             // scheduler queue length
  b.add("in-progress",
        VPackValue(0));                 // number of working (non-idle) threads
  b.add("direct-exec", VPackValue(0));  // obsolete
}
Scheduler::QueueStatistics ThreadPoolScheduler::queueStatistics() const {
  return QueueStatistics();
}
void ThreadPoolScheduler::trackCreateHandlerTask() noexcept {}
void ThreadPoolScheduler::trackBeginOngoingLowPriorityTask() noexcept {}
void ThreadPoolScheduler::trackEndOngoingLowPriorityTask() noexcept {}
void ThreadPoolScheduler::trackQueueTimeViolation() {}
void ThreadPoolScheduler::trackQueueItemSize(std::int64_t int64) noexcept {}

uint64_t ThreadPoolScheduler::getLastLowPriorityDequeueTime() const noexcept {
  return _lastLowPriorityDequeueTime.load(std::memory_order::relaxed);
}

void ThreadPoolScheduler::setLastLowPriorityDequeueTime(
    uint64_t time) noexcept {
  _lastLowPriorityDequeueTime.store(time, std::memory_order::relaxed);
}

std::pair<uint64_t, uint64_t>
ThreadPoolScheduler::getNumberLowPrioOngoingAndQueued() const {
  auto queued = _threadPools[1]->statistics.queued.load();
  auto done = _threadPools[1]->statistics.done.load();
  return std::make_pair(done - queued, queued);
}

double ThreadPoolScheduler::approximateQueueFillGrade() const { return 0; }

double ThreadPoolScheduler::unavailabilityQueueFillGrade() const { return 0; }

bool ThreadPoolScheduler::queueItem(RequestLane lane,
                                    std::unique_ptr<WorkItemBase> item,
                                    bool bounded) {
  auto prio = PriorityRequestLane(lane);
  _threadPools[int(prio)]->push(std::move(item));
  return true;
}

constexpr uint64_t approxWorkerStackSize = 4'000'000;

ThreadPoolScheduler::ThreadPoolScheduler(
    ArangodServer& server, uint64_t maxThreads,
    std::shared_ptr<SchedulerMetrics> metrics)
    : Scheduler(server), _metrics(std::move(metrics)) {
  _threadPools.reserve(4);
  _threadPools.emplace_back(std::make_unique<ThreadPool>(
      "SchedMaintenance", std::max(std::ceil(maxThreads * 0.1), 2.)));
  _threadPools.emplace_back(std::make_unique<ThreadPool>(
      "SchedLow", std::max(std::ceil(maxThreads * 0.6), 16.)));
  _threadPools.emplace_back(std::make_unique<ThreadPool>(
      "SchedMedium", std::max(std::ceil(maxThreads * 0.4), 8.)));
  _threadPools.emplace_back(std::make_unique<ThreadPool>(
      "SchedHigh", std::max(std::ceil(maxThreads * 0.4), 8.)));

  _metrics->_metricsStackMemoryWorkerThreads =
      approxWorkerStackSize *
      std::accumulate(_threadPools.begin(), _threadPools.end(), 0,
                      [](std::size_t accum, auto const& pool) {
                        return accum + pool->numThreads;
                      });
}

void ThreadPoolScheduler::shutdown() {
  _stopping = true;
  Scheduler::shutdown();
}
