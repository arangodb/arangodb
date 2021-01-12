////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "InitialSyncer.h"

#include "Basics/FunctionUtils.h"
#include "Basics/application-exit.h"
#include "Logger/LogMacros.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"

namespace arangodb {

InitialSyncer::InitialSyncer(ReplicationApplierConfiguration const& configuration,
                             replutils::ProgressInfo::Setter setter)
    : Syncer(configuration), _progress{setter} {}

InitialSyncer::~InitialSyncer() {
  _batchPingTimer.reset();

  try {
    if (!_state.isChildSyncer) {
      _batch.finish(_state.connection, _progress, _state.syncerId);
    }
  } catch (...) {
  }
}

/// @brief start a recurring task to extend the batch
void InitialSyncer::startRecurringBatchExtension() {
  TRI_ASSERT(!_state.isChildSyncer);
  if (isAborted()) {
    _batchPingTimer.reset();
    return;
  }

  int secs = _batch.ttl / 2;
  if (secs < 30) {
    secs = 30;
  }
        
  std::weak_ptr<Syncer> self(shared_from_this());
  bool queued = false;
  std::tie(queued, _batchPingTimer) =
      basics::function_utils::retryUntilTimeout<Scheduler::WorkHandle>(
          [secs, self]() -> std::pair<bool, Scheduler::WorkHandle> {
            return SchedulerFeature::SCHEDULER->queueDelay(
                RequestLane::SERVER_REPLICATION, std::chrono::seconds(secs),
                [self](bool cancelled) {
                  if (!cancelled) {
                    auto syncer = self.lock();
                    if (syncer) {
                      auto* s = static_cast<InitialSyncer*>(syncer.get());
                      if (s->_batch.id != 0 && !s->isAborted()) {
                        s->_batch.extend(s->_state.connection, s->_progress,
                                         s->_state.syncerId);
                        s->startRecurringBatchExtension();
                      }
                    }
                  }
                });
          },
          Logger::REPLICATION, "queue batch extension");
  if (!queued) {
    LOG_TOPIC("f8b3e", ERR, Logger::REPLICATION)
        << "Failed to queue replication batch extension for 5 minutes, exiting.";
    // don't abort, as this is not a critical error 
    // if requeueing has failed here, the replication can still go on, but
    // it _may_ fail later because the batch has expired on the leader.
    // but there are still chances it can continue successfully
  }
}

}  // namespace arangodb
