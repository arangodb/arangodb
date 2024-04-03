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

using namespace arangodb;

void ThreadPoolScheduler::toVelocyPack(velocypack::Builder& builder) const {}
Scheduler::QueueStatistics ThreadPoolScheduler::queueStatistics() const {
  return QueueStatistics();
}
void ThreadPoolScheduler::trackCreateHandlerTask() noexcept {}
void ThreadPoolScheduler::trackBeginOngoingLowPriorityTask() noexcept {}
void ThreadPoolScheduler::trackEndOngoingLowPriorityTask() noexcept {}
void ThreadPoolScheduler::trackQueueTimeViolation() {}
void ThreadPoolScheduler::trackQueueItemSize(std::int64_t int64) noexcept {}

uint64_t ThreadPoolScheduler::getLastLowPriorityDequeueTime() const noexcept {
  return 0;
}

void ThreadPoolScheduler::setLastLowPriorityDequeueTime(
    uint64_t time) noexcept {}

std::pair<uint64_t, uint64_t>
ThreadPoolScheduler::getNumberLowPrioOngoingAndQueued() const {
  return std::pair<uint64_t, uint64_t>();
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

ThreadPoolScheduler::ThreadPoolScheduler(ArangodServer& server,
                                         uint64_t maxThreads)
    : Scheduler(server) {
  _threadPools.reserve(4);
  _threadPools.emplace_back(
      std::make_unique<ThreadPool>(std::min(std::ceil(maxThreads * 0.1), 2.)));
  _threadPools.emplace_back(
      std::make_unique<ThreadPool>(std::min(std::ceil(maxThreads * 0.6), 8.)));
  _threadPools.emplace_back(
      std::make_unique<ThreadPool>(std::min(std::ceil(maxThreads * 0.4), 4.)));
  _threadPools.emplace_back(
      std::make_unique<ThreadPool>(std::min(std::ceil(maxThreads * 0.4), 4.)));
}

void ThreadPoolScheduler::shutdown() {
  _stopping = true;
  Scheduler::shutdown();
}
