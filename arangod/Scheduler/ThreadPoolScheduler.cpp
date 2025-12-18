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
#include "ThreadPoolScheduler.h"

#include <velocypack/Builder.h>
#include <numeric>

#include "Metrics/Gauge.h"
#include "Metrics/Counter.h"

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

void ThreadPoolScheduler::trackQueueItemSize(std::int64_t x) noexcept {
  _metrics->_schedulerQueueMemory += x;
}

uint64_t ThreadPoolScheduler::getLastLowPriorityDequeueTime() const noexcept {
  return _lastLowPriorityDequeueTime.load(std::memory_order::relaxed);
}

void ThreadPoolScheduler::setLastLowPriorityDequeueTime(
    uint64_t time) noexcept {
  _lastLowPriorityDequeueTime.store(time, std::memory_order::relaxed);
}

bool ThreadPoolScheduler::queueItem(RequestLane lane,
                                    std::unique_ptr<WorkItemBase> item) {
  item->enqueueTime = std::chrono::steady_clock::now();
  auto prio = PriorityRequestLane(lane);
  _threadPools[int(prio)]->push(std::move(item));
  return true;
}

ThreadPoolScheduler::ThreadPoolScheduler(
    ArangodServer& server, uint64_t maxThreads,
    std::shared_ptr<SchedulerMetrics> metrics)
    : Scheduler(server), _metrics(std::move(metrics)) {
  ThreadPoolMetrics poolMetrics = {
      .jobsDone = &_metrics->_metricsJobsDoneTotal,
      .jobsQueued = &_metrics->_metricsJobsSubmittedTotal,
      .jobsDequeued = &_metrics->_metricsJobsDequeuedTotal,
  };

  _threadPools.reserve(4);

  // Note: in total we allocate 1.5 times the number of requested threads,
  // because in this scheduler implementation we no longer share threads between
  // the different priorities, so we want to compensate that a bit.
  poolMetrics.queueLength = _metrics->_metricsQueueLengths[0];
  poolMetrics.dequeueTimes = _metrics->_metricsDequeueTimes[0];
  _threadPools.emplace_back(std::make_unique<ThreadPool>(
      "SchedMaintnc", std::max<std::size_t>(std::ceil(maxThreads * 0.1), 2),
      poolMetrics));

  poolMetrics.queueLength = _metrics->_metricsQueueLengths[1];
  poolMetrics.dequeueTimes = _metrics->_metricsDequeueTimes[1];
  _threadPools.emplace_back(std::make_unique<ThreadPool>(
      "SchedHigh", std::max<std::size_t>(std::ceil(maxThreads * 0.4), 8),
      poolMetrics));

  poolMetrics.queueLength = _metrics->_metricsQueueLengths[2];
  poolMetrics.dequeueTimes = _metrics->_metricsDequeueTimes[2];
  _threadPools.emplace_back(std::make_unique<ThreadPool>(
      "SchedMedium", std::max<std::size_t>(std::ceil(maxThreads * 0.4), 8),
      poolMetrics));

  poolMetrics.queueLength = _metrics->_metricsQueueLengths[3];
  poolMetrics.dequeueTimes = _metrics->_metricsDequeueTimes[3];
  _threadPools.emplace_back(std::make_unique<ThreadPool>(
      "SchedLow", std::max<std::size_t>(std::ceil(maxThreads * 0.6), 16),
      poolMetrics));
}

void ThreadPoolScheduler::shutdown() {
  _stopping = true;
  Scheduler::shutdown();
  for (auto& pool : _threadPools) {
    pool->shutdown();
  }
}
