////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"

namespace arangodb {
  
InitialSyncer::InitialSyncer(
    ReplicationApplierConfiguration const& configuration,
    replutils::ProgressInfo::Setter setter)
    : Syncer(configuration),
      _progress{setter} {}

InitialSyncer::~InitialSyncer() {
  if (_batchPingTimer) {
    try {
      _batchPingTimer->cancel();
    } catch (...) {
      // cancel may throw
    }
  }
  
  try {
    if (!_state.isChildSyncer) {
      _batch.finish(_state.connection, _progress);
    }
  } catch (...) {}
}
  
/// @brief start a recurring task to extend the batch
void InitialSyncer::startRecurringBatchExtension() {
  TRI_ASSERT(!_state.isChildSyncer);
  if (isAborted()) {
    return;
  }
  
  if (!_batchPingTimer) {
    _batchPingTimer.reset(SchedulerFeature::SCHEDULER->newSteadyTimer());
  }
  
  int secs = _batch.ttl / 2;
  if (secs < 30) {
    secs = 30;
  }
  _batchPingTimer->expires_after(std::chrono::seconds(secs));
  _batchPingTimer->async_wait([this](asio_ns::error_code ec) {
    if (!ec && _batch.id != 0 && !isAborted()) {
      _batch.extend(_state.connection, _progress);
      startRecurringBatchExtension();
    }
  });
}

}  // namespace arangodb
