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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Scheduler/Scheduler.h"

namespace arangodb::tests {

struct FakeScheduler : Scheduler {
  // Scheduler virtual methods
  bool queueItem(RequestLane lane, std::unique_ptr<WorkItemBase> item,
                 bool bounded) override;
  void toVelocyPack(velocypack::Builder& builder) const override;
  QueueStatistics queueStatistics() const override;

  void trackCreateHandlerTask() noexcept override;
  void trackBeginOngoingLowPriorityTask() noexcept override;
  void trackEndOngoingLowPriorityTask() noexcept override;
  void trackQueueTimeViolation() override;
  void trackQueueItemSize(std::int64_t) noexcept override;
  uint64_t getLastLowPriorityDequeueTime() const noexcept override;
  void setLastLowPriorityDequeueTime(uint64_t time) noexcept override;
  std::pair<uint64_t, uint64_t> getNumberLowPrioOngoingAndQueued()
      const override;

  double approximateQueueFillGrade() const override;
  double unavailabilityQueueFillGrade() const override;
  bool isStopping() override;

  // FakeScheduler specific methods
  explicit FakeScheduler(ArangodServer& server);
  ~FakeScheduler();
  bool queueEmpty();
  std::size_t queueSize();
  void runOnce();
  void runOne(std::size_t idx);

  std::vector<std::unique_ptr<WorkItemBase>> _queue;
};

}  // namespace arangodb::tests
