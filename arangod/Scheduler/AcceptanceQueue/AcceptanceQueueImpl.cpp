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
///
/// @author Johann Listunov
////////////////////////////////////////////////////////////////////////////////

#include "AcceptanceQueueImpl.h"

#include "ApplicationFeatures/ApplicationServer.h"

#include "Cluster/ServerState.h"

#include "Logger/LogMacros.h"
#include "Logger/Logger.h"

#include "Network/NetworkFeature.h"

#include "Metrics/Counter.h"
#include "Metrics/Gauge.h"

namespace {
// constexpr uint64_t MaintenancePriorityQueue = 0;
// constexpr uint64_t HighPriorityQueue = 1;
// constexpr uint64_t MediumPriorityQueue = 2;
constexpr uint64_t LowPriorityQueue = 3;

using time_point = std::chrono::time_point<std::chrono::steady_clock>;
time_point lastQueueFullWarning[arangodb::AcceptanceQueueImpl::NumberOfQueues];
int64_t fullQueueEvents[arangodb::AcceptanceQueueImpl::NumberOfQueues] = {0};
std::mutex fullQueueWarningMutex[arangodb::AcceptanceQueueImpl::NumberOfQueues];

void logQueueFullEveryNowAndThen(uint64_t const queueNo,
                                 uint64_t const maxQueueSize) {
  auto const& now = std::chrono::steady_clock::now();
  uint64_t events;
  bool printLog = false;

  {
    std::unique_lock guard(fullQueueWarningMutex[queueNo]);
    events = ++fullQueueEvents[queueNo];
    if (now - lastQueueFullWarning[queueNo] > std::chrono::seconds(10)) {
      printLog = true;
      lastQueueFullWarning[queueNo] = now;
      fullQueueEvents[queueNo] = 0;
    }
  }

  if (printLog) {
    LOG_TOPIC("dead1", WARN, arangodb::Logger::THREADS)
        << "Scheduler queue " << queueNo << " with max capacity "
        << maxQueueSize << " is full (happened " << events
        << " times since last message)";
  }
}

}  // namespace

namespace arangodb {
AcceptanceQueueImpl::AcceptanceQueueImpl(
    uint64_t const maxQueueSize, uint64_t const fifo1Size,
    uint64_t const fifo2Size, uint64_t const fifo3Size,
    double const unavailabilityQueueFillGrade, Scheduler* scheduler,
    std::shared_ptr<SchedulerMetrics> const& metrics,
    std::shared_ptr<LowPrioAntiOverwhelm> const& antiOverwhelm)
    : _maxFifoSizes{maxQueueSize, fifo1Size, fifo2Size, fifo3Size},
      _unavailabilityQueueFillGrade(unavailabilityQueueFillGrade),
      _scheduler(scheduler),
      _metrics(metrics),
      _antiOverwhelm(antiOverwhelm) {}

bool AcceptanceQueueImpl::start() {
  // the scheduler starts first
  return true;
}

void AcceptanceQueueImpl::shutdown() {}

void AcceptanceQueueImpl::setLastLowPriorityDequeueTime(uint64_t const time) {
  _scheduler->setLastLowPriorityDequeueTime(time);
}

uint64_t AcceptanceQueueImpl::getLastLowPriorityDequeueTime() const noexcept {
  return _metrics->_metricsLastLowPriorityDequeueTime.load();
}

Scheduler::QueueStatistics AcceptanceQueueImpl::queueStatistics() const {
  return _scheduler->queueStatistics();
}

void AcceptanceQueueImpl::trackBeginOngoingLowPriorityTask() noexcept {
  _metrics->_ongoingLowPriorityGauge += 1;
}

void AcceptanceQueueImpl::trackEndOngoingLowPriorityTask() noexcept {
  _metrics->_ongoingLowPriorityGauge -= 1;
}

void AcceptanceQueueImpl::trackCreateHandlerTask() noexcept {
  ++_metrics->_metricsHandlerTasksCreated;
}

bool AcceptanceQueueImpl::trackQueueTimeViolation(
    double const requestedQueueTime) noexcept {
  double const lastDequeueTime =
      static_cast<double>(getLastLowPriorityDequeueTime()) / 1000.0;
  if (lastDequeueTime > requestedQueueTime) {
    ++_metrics->_metricsQueueTimeViolations;
    // the log topic should actually be REQUESTS here, but the default log
    // level for the REQUESTS log topic is FATAL, so if we logged here in
    // INFO level, it would effectively be suppressed. thus we are using the
    // Scheduler's log topic here, which is somewhat related.
    LOG_TOPIC("1bbcc", WARN, Logger::THREADS)
        << "dropping incoming request because the client-specified maximum "
           "queue time requirement ("
        << requestedQueueTime << "s) would be violated by current queue time ("
        << lastDequeueTime << "s)";
    return true;
  }
  return false;
}

void AcceptanceQueueImpl::trackQueueItemSize(std::int64_t const x) noexcept {
  _metrics->_schedulerQueueMemory += x;
}

[[nodiscard]] bool AcceptanceQueueImpl::queueItem(
    std::unique_ptr<AcceptanceQueueWorkItemBase> item) {
  auto const queueNo = static_cast<size_t>(PriorityRequestLane(item->lane()));
  TRI_ASSERT(queueNo < NumberOfQueues);

  if (!trackWorkItemAddedToQueue(item->lane())) {
    // could not track item, means the queue is full
    auto const maxSize = _maxFifoSizes[queueNo];
    logQueueFullEveryNowAndThen(queueNo, maxSize);
    return false;
  }
  item->queued();
  return _scheduler->queueItem(item->lane(), std::move(item));
}

bool AcceptanceQueueImpl::trackWorkItemAddedToQueue(RequestLane const lane) {
  // Check if we still have space in the queue for this PriorityRequestLane
  auto const queueNo = static_cast<size_t>(PriorityRequestLane(lane));
  TRI_ASSERT(queueNo < NumberOfQueues);
  auto& numTotalCountedItems = _numScheduledItems[queueNo];
  auto const maxSize = _maxFifoSizes[queueNo];
  if (numTotalCountedItems.fetch_add(1, std::memory_order_relaxed) > maxSize) {
    numTotalCountedItems.fetch_sub(1, std::memory_order_relaxed);
    LOG_TOPIC("98d94", DEBUG, Logger::THREADS)
        << "unable to push job to scheduler queue: queue is full";
    ++_metrics->_metricsQueueFull;
    return false;
  }
  *_metrics->_metricsQueueLengths[queueNo] += 1;
  return true;
}

bool AcceptanceQueueImpl::trackWorkItemRemovedFromQueue(
    RequestLane const lane) {
  auto const queueNo = static_cast<size_t>(PriorityRequestLane(lane));
  TRI_ASSERT(queueNo < NumberOfQueues);
  _numScheduledItems[queueNo].fetch_sub(1, std::memory_order_relaxed);
  *_metrics->_metricsQueueLengths[queueNo] -= 1;
  return true;
}

double AcceptanceQueueImpl::approximateQueueFillGrade() const {
  uint64_t const maxLength = _maxFifoSizes[LowPriorityQueue];
  uint64_t const qLength =
      std::min<uint64_t>(maxLength, _metrics->_metricsQueueLength.load());
  TRI_ASSERT(maxLength > 0);
  return static_cast<double>(qLength) / static_cast<double>(maxLength);
}

std::pair<uint64_t, uint64_t>
AcceptanceQueueImpl::getNumberLowPrioOngoingAndQueued() const {
  return {_metrics->_ongoingLowPriorityGauge.load(std::memory_order_relaxed),
          _metrics->_metricsQueueLengths[LowPriorityQueue]->load(
              std::memory_order_relaxed)};
}

double AcceptanceQueueImpl::unavailabilityQueueFillGrade() const {
  return _unavailabilityQueueFillGrade;
}

}  // namespace arangodb