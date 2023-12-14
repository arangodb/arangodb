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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#include "Scheduler.h"

#include "Assertions/Assert.h"
#include "Scheduler/Scheduler.h"

namespace arangodb::replication2::replicated_state::document::actor {

Scheduler::Scheduler(std::shared_ptr<replication2::IScheduler> scheduler)
    : _scheduler{std::move(scheduler)} {
  TRI_ASSERT(_scheduler != nullptr);
}

void Scheduler::queue(arangodb::actor::LazyWorker&& worker) {
  _scheduler->queue(std::move(worker));
}

void Scheduler::delay(std::chrono::seconds delay,
                      std::function<void(bool)>&& fn) {
  std::ignore =
      _scheduler->queueDelayed("replication2-actors", delay, std::move(fn));
}

}  // namespace arangodb::replication2::replicated_state::document::actor
