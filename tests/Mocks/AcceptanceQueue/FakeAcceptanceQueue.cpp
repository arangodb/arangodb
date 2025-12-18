///////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License"){}
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

#include "FakeAcceptanceQueue.h"

namespace arangodb::tests {

FakeAcceptanceQueue::FakeAcceptanceQueue() = default;
FakeAcceptanceQueue::~FakeAcceptanceQueue() {
  ADB_PROD_ASSERT(_queue.empty() && _currentQueueItemSize == 0);
};

bool FakeAcceptanceQueue::queueEmpty() const noexcept { return _queue.empty(); }

std::size_t FakeAcceptanceQueue::queueSize() const noexcept {
  return _queue.size();
}

void FakeAcceptanceQueue::runOnce() { runOne(0); }

void FakeAcceptanceQueue::runOne(std::size_t const idx) {
  TRI_ASSERT(idx < _queue.size());
  auto it = _queue.begin();
  std::advance(it, idx);
  auto item = std::move(*it);
  _queue.erase(it);
  item->invoke();
}

bool FakeAcceptanceQueue::queueItem(
    std::unique_ptr<AcceptanceQueueWorkItemBase> item) {
  _queue.push_back(std::move(item));
  return true;
}

void FakeAcceptanceQueue::setLastLowPriorityDequeueTime(uint64_t) {
  ADB_PROD_ASSERT(false) << "not implemented";
}
uint64_t FakeAcceptanceQueue::getLastLowPriorityDequeueTime() const noexcept {
  ADB_PROD_ASSERT(false) << "not implemented";
  return {};
}

Scheduler::QueueStatistics FakeAcceptanceQueue::queueStatistics() const {
  ADB_PROD_ASSERT(false) << "not implemented";
  return {};
}

bool FakeAcceptanceQueue::start() {
  ADB_PROD_ASSERT(false) << "not implemented";
  return {};
}

void FakeAcceptanceQueue::shutdown() {
  ADB_PROD_ASSERT(false) << "not implemented";
}

void FakeAcceptanceQueue::trackBeginOngoingLowPriorityTask() noexcept {
  ADB_PROD_ASSERT(false) << "not implemented";
}

void FakeAcceptanceQueue::trackEndOngoingLowPriorityTask() noexcept {
  ADB_PROD_ASSERT(false) << "not implemented";
}

void FakeAcceptanceQueue::trackCreateHandlerTask() noexcept {
  ADB_PROD_ASSERT(false) << "not implemented";
}

bool FakeAcceptanceQueue::trackQueueTimeViolation(double const) noexcept {
  ADB_PROD_ASSERT(false) << "not implemented";
  return false;
}

void FakeAcceptanceQueue::trackQueueItemSize(std::int64_t const size) noexcept {
  _currentQueueItemSize += size;
}

bool FakeAcceptanceQueue::trackWorkItemRemovedFromQueue(RequestLane lane) {
  return true;
}

std::pair<uint64_t, uint64_t>
FakeAcceptanceQueue::getNumberLowPrioOngoingAndQueued() const {
  return {0, 0};
}

double FakeAcceptanceQueue::approximateQueueFillGrade() const { return 0.0; }

double FakeAcceptanceQueue::unavailabilityQueueFillGrade() const { return 0.0; }

}  // namespace arangodb::tests