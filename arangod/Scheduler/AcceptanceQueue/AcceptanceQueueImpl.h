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

#pragma once

#include "AcceptanceQueue.h"

#include "LowPrioAntiOverwhelm.h"

#include "../SchedulerMetrics.h"

namespace arangodb {

class AcceptanceQueueImpl final : public AcceptanceQueue {
 public:
  constexpr static uint64_t NumberOfQueues = 4;

  explicit AcceptanceQueueImpl(
      uint64_t maxQueueSize, uint64_t fifo1Size, uint64_t fifo2Size,
      uint64_t fifo3Size, double unavailabilityQueueFillGrade,
      Scheduler* scheduler, std::shared_ptr<SchedulerMetrics> const& metrics,
      std::shared_ptr<LowPrioAntiOverwhelm> const& antiOverwhelm);

  ~AcceptanceQueueImpl() override = default;

  [[nodiscard]] bool queueItem(
      std::unique_ptr<AcceptanceQueueWorkItemBase> item) override;

  /// @brief set the time it took for the last low prio item to be dequeued
  /// (time between queuing and dequeing) [ms]
  void setLastLowPriorityDequeueTime(uint64_t time) override;

  /// @brief returns the last stored dequeue time [ms]
  [[nodiscard]] uint64_t getLastLowPriorityDequeueTime()
      const noexcept override;

  [[nodiscard]] Scheduler::QueueStatistics queueStatistics() const override;

  bool start() override;
  void shutdown() override;

  void trackBeginOngoingLowPriorityTask() noexcept override;
  void trackEndOngoingLowPriorityTask() noexcept override;

  void trackCreateHandlerTask() noexcept override;
  bool trackQueueTimeViolation(double) noexcept override;

  void trackQueueItemSize(std::int64_t x) noexcept override;

  std::pair<uint64_t, uint64_t> getNumberLowPrioOngoingAndQueued()
      const override;
  double approximateQueueFillGrade() const override;
  double unavailabilityQueueFillGrade() const override;

 private:
  bool trackWorkItemRemovedFromQueue(RequestLane lane) override;
  bool trackWorkItemAddedToQueue(RequestLane lane);

  /// @brief the number of items that have been enqueued via tryBoundedQueue
  std::atomic<uint64_t> _numScheduledItems[NumberOfQueues]{0};

  // this limit leads to new work being rejected
  uint64_t const _maxFifoSizes[NumberOfQueues];

  /// @brief fill grade of the scheduler's queue (in %) from which onwards
  /// the server is considered unavailable (because of overload)
  double const _unavailabilityQueueFillGrade;

  Scheduler* _scheduler;
  std::shared_ptr<SchedulerMetrics> const _metrics;
  std::shared_ptr<LowPrioAntiOverwhelm> const _antiOverwhelm;
};

}  // namespace arangodb
