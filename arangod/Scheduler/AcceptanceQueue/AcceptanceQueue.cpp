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

#include "AcceptanceQueue.h"

#include "Metrics/Gauge.h"

namespace arangodb {
AcceptanceQueue::AcceptanceQueue(
    Scheduler* scheduler, const std::shared_ptr<SchedulerMetrics>& metrics)
    : _scheduler(scheduler), _metrics(metrics) {}

bool AcceptanceQueue::start() {
  // the scheduler starts first
  return true;
}

void AcceptanceQueue::shutdown() {
  // the scheduler will shutdown after this
}

void AcceptanceQueue::setLastLowPriorityDequeueTime(uint64_t time) {
  _scheduler->setLastLowPriorityDequeueTime(time);
}

Scheduler::QueueStatistics AcceptanceQueue::queueStatistics() const {
  return _scheduler->queueStatistics();
}

void AcceptanceQueue::trackBeginOngoingLowPriorityTask() noexcept {
  _metrics->_ongoingLowPriorityGauge += 1;
}
void AcceptanceQueue::trackEndOngoingLowPriorityTask() noexcept {
  _metrics->_ongoingLowPriorityGauge -= 1;
}

}  // namespace arangodb