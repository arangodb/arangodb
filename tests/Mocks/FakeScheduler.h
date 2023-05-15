////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2023-2023 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
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
  uint64_t getLastLowPriorityDequeueTime() const noexcept override;
  double approximateQueueFillGrade() const override;
  double unavailabilityQueueFillGrade() const override;
  bool isStopping() override;

  // FakeScheduler specific methods
  explicit FakeScheduler(ArangodServer& server);
  ~FakeScheduler();
  bool queueEmpty();
  std::size_t queueSize();
  void runOnce();

  std::queue<std::unique_ptr<WorkItemBase>> _queue;
};

}  // namespace arangodb::tests
