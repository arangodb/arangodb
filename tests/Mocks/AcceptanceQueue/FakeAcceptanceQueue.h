///////////////////////////////////////////////////////////////////////////////
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

#pragma once

#include "Scheduler/AcceptanceQueue/AcceptanceQueue.h"

namespace arangodb::tests {
struct FakeAcceptanceQueue final : AcceptanceQueue {
  FakeAcceptanceQueue();
  ~FakeAcceptanceQueue() override;

  [[nodiscard]] bool queueEmpty() const noexcept;
  [[nodiscard]] std::size_t queueSize() const noexcept;
  void runOnce();
  void runOne(std::size_t idx);

  std::vector<std::unique_ptr<Scheduler::WorkItemBase>> _queue;
  std::int64_t _currentQueueItemSize{0};
  // AcceptanceQueue overrides

  [[nodiscard]] bool queueItem(
      std::unique_ptr<AcceptanceQueueWorkItemBase> item) override;
  void setLastLowPriorityDequeueTime(uint64_t time) override;
  [[nodiscard]] uint64_t getLastLowPriorityDequeueTime()
      const noexcept override;
  [[nodiscard]] Scheduler::QueueStatistics queueStatistics() const override;
  bool start() override;
  void shutdown() override;
  void trackBeginOngoingLowPriorityTask() noexcept override;
  void trackEndOngoingLowPriorityTask() noexcept override;
  void trackCreateHandlerTask() noexcept override;
  bool trackQueueTimeViolation(double) noexcept override;
  void trackQueueItemSize(std::int64_t size) noexcept override;
  bool trackWorkItemRemovedFromQueue(RequestLane lane) override;
  std::pair<uint64_t, uint64_t> getNumberLowPrioOngoingAndQueued()
      const override;
  double approximateQueueFillGrade() const override;
  double unavailabilityQueueFillGrade() const override;
};

}  // namespace arangodb::tests