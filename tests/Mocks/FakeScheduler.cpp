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

#include "FakeScheduler.h"

#include <gtest/gtest.h>

#include "Basics/SourceLocation.h"
#include "Logger/LogMacros.h"

namespace arangodb {
namespace tests {

bool FakeScheduler::queueItem(RequestLane lane,
                              std::unique_ptr<WorkItemBase> item,
                              bool bounded) {
  _queue.emplace(std::move(item));
  return true;
}

void FakeScheduler::toVelocyPack(velocypack::Builder& builder) const {
  ADB_PROD_ASSERT(false) << "not implemented";
}

Scheduler::QueueStatistics FakeScheduler::queueStatistics() const {
  ADB_PROD_ASSERT(false) << "not implemented";
  return QueueStatistics();
}

uint64_t FakeScheduler::getLastLowPriorityDequeueTime() const noexcept {
  ADB_PROD_ASSERT(false) << "not implemented";
  return 0;
}

double FakeScheduler::approximateQueueFillGrade() const {
  ADB_PROD_ASSERT(false) << "not implemented";
  return 0;
}

double FakeScheduler::unavailabilityQueueFillGrade() const {
  ADB_PROD_ASSERT(false) << "not implemented";
  return 0;
}

bool FakeScheduler::isStopping() {
  ADB_PROD_ASSERT(false) << "not implemented";
  return false;
}

FakeScheduler::FakeScheduler(ArangodServer& server) : Scheduler(server) {}

FakeScheduler::~FakeScheduler() { ADB_PROD_ASSERT(queueEmpty()); }

bool FakeScheduler::queueEmpty() { return _queue.empty(); }

std::size_t FakeScheduler::queueSize() { return _queue.size(); }

void FakeScheduler::runOnce() {
  TRI_ASSERT(!queueEmpty());
  auto item = std::move(_queue.front());
  _queue.pop();
  item->invoke();
}

}  // namespace tests
}  // namespace arangodb
