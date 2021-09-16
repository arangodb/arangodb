////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/ShutdownFeature.h"
#include "FeaturePhases/AgencyFeaturePhase.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "Logger/Logger.h"
#include "Logger/LoggerFeature.h"
#include "Pregel/PregelFeature.h"
#include "RestServer/ConsoleFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/MetricsFeature.h"
#include "RestServer/ScriptFeature.h"
#include "RestServer/SoftShutdownFeature.h"
#include "Scheduler/SchedulerFeature.h"
#include "Scheduler/SupervisedScheduler.h"
#include "Transaction/Manager.h"
#include "Transaction/ManagerFeature.h"
#include "Utils/CursorRepository.h"
#include "VocBase/vocbase.h"

using namespace arangodb::options;

namespace {

void queueShutdownChecker(std::mutex& mutex,
    arangodb::Scheduler::WorkHandle& workItem,
    std::function<void(bool)>& checkFunc) {

  arangodb::Scheduler* scheduler = arangodb::SchedulerFeature::SCHEDULER;
  std::lock_guard<std::mutex> guard(mutex);
  workItem = scheduler->queueDelayed(arangodb::RequestLane::CLUSTER_INTERNAL,
                                     std::chrono::seconds(2), checkFunc);
}

}

namespace arangodb {

SoftShutdownFeature::SoftShutdownFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "SoftShutdown") {
  setOptional(true);
  startsAfter<application_features::AgencyFeaturePhase>();
  startsAfter<ShutdownFeature>();
  startsAfter<ConsoleFeature>();
  startsAfter<ScriptFeature>();

  // We do not yet know if we are a coordinator, so just in case,
  // create a SoftShutdownTracker, it will not hurt if it is not used:
  _softShutdownTracker = std::make_shared<SoftShutdownTracker>(server);
}

void SoftShutdownFeature::beginShutdown() {
  _softShutdownTracker->cancelChecker();
}

void SoftShutdownTracker::cancelChecker() {
  if (_softShutdownOngoing.load(std::memory_order_relaxed)) {
    // This is called when an actual shutdown happens. We then want to
    // delete the WorkItem of the soft shutdown checker, such that the
    // Scheduler does not have any cron jobs any more:
    std::lock_guard<std::mutex> guard(_workItemMutex);
    _workItem.reset();
  }
}

SoftShutdownTracker::SoftShutdownTracker(
    application_features::ApplicationServer& server)
  : _server(server), _softShutdownOngoing(false) {
  _checkFunc = [this](bool cancelled) {
    if (_server.isStopping()) {
      return;   // already stopping, do nothing, and in particular
                // let's not schedule ourselves again!
    }
    if (!this->checkAndShutdownIfAllClear()) {
      // Rearm ourselves:
      queueShutdownChecker(this->_workItemMutex, this->_workItem,
                           this->_checkFunc);
    }
  };
}

void SoftShutdownTracker::initiateSoftShutdown() {
  bool prev = _softShutdownOngoing.exchange(true, std::memory_order_relaxed);
  if (prev) {
    // Make behaviour idempotent!
    LOG_TOPIC("cce32", INFO, Logger::STARTUP)
        << "Received second soft shutdown request, ignoring it...";
    return;
  }

  LOG_TOPIC("fedd2", INFO, Logger::STARTUP)
      << "Initiating soft shutdown...";

  // Tell GeneralServerFeature, which will forward to all features which
  // overload the initiateSoftShutdown method:
  _server.initiateSoftShutdown();
  // Currently, these are:
  //   - the GeneralServerFeature for its JobManager
  //   - the PregelFeature

  // And initiate our checker to watch numbers:
  queueShutdownChecker(_workItemMutex, _workItem, _checkFunc);
}
    
bool SoftShutdownTracker::checkAndShutdownIfAllClear() const {
  Status status = getStatus();
  if (!status.allClear()) {
    VPackBuilder builder;
    toVelocyPack(builder, status);
    // FIXME: Set to DEBUG level
    LOG_TOPIC("ffeec", INFO, Logger::STARTUP)
        << "Soft shutdown check said 'not all clear': "
        << builder.slice().toJson() << ".";
    return false;
  }
  LOG_TOPIC("ffeed", INFO, Logger::STARTUP)
      << "Goal reached for soft shutdown, all ongoing tasks are terminated"
         ", will now trigger the actual shutdown...";
  initiateActualShutdown();
  return true;
}

void SoftShutdownTracker::initiateActualShutdown() const {
  Scheduler* scheduler = SchedulerFeature::SCHEDULER;
  auto self = shared_from_this();
  scheduler->queue(RequestLane::CLUSTER_INTERNAL, [self = shared_from_this()] {
    // Give the server 2 seconds to finish stuff
    std::this_thread::sleep_for(std::chrono::seconds(2));
    self->_server.beginShutdown();
  });
}

void SoftShutdownTracker::toVelocyPack(VPackBuilder& builder,
    SoftShutdownTracker::Status const& status) {
  VPackObjectBuilder guard(&builder);
  builder.add("softShutdownOngoing", VPackValue(status.softShutdownOngoing));
  builder.add("AQLcursors", VPackValue(status.AQLcursors));
  builder.add("transactions", VPackValue(status.transactions));
  builder.add("pendingJobs", VPackValue(status.pendingJobs));
  builder.add("doneJobs", VPackValue(status.doneJobs));
  builder.add("pregelConductors", VPackValue(status.pregelConductors));
  builder.add("lowPrioOngoingRequests", VPackValue(status.lowPrioOngoingRequests));
  builder.add("lowPrioQueuedRequests", VPackValue(status.lowPrioQueuedRequests));
  builder.add("allClear", VPackValue(status.allClear()));
}

SoftShutdownTracker::Status SoftShutdownTracker::getStatus() const {
  Status status(_softShutdownOngoing.load(std::memory_order_relaxed));

  // Get number of active AQL cursors from each database:
  auto& databaseFeature = _server.getFeature<DatabaseFeature>();
  databaseFeature.enumerate([&status](TRI_vocbase_t* vocbase) {
        CursorRepository* repo = vocbase->cursorRepository();
        status.AQLcursors += repo->count();
      });

  // Get number of active transactions from Manager:
  auto& managerFeature = _server.getFeature<transaction::ManagerFeature>();
  auto* manager = managerFeature.manager();
  if (manager != nullptr) {
    status.transactions = manager->getActiveTransactionCount();
  }

  // Get numbers of pending and done asynchronous jobs:
  auto& generalServerFeature = _server.getFeature<GeneralServerFeature>();
  auto& jobManager = generalServerFeature.jobManager();
  std::tie(status.pendingJobs, status.doneJobs)
      = jobManager.getNrPendingAndDone();

  // Get number of active Pregel conductors on this coordinator:
  auto& pregelFeature = _server.getFeature<pregel::PregelFeature>();
  status.pregelConductors = pregelFeature.numberOfActiveConductors();

  // Get number of ongoing and queued requests from scheduler:
  std::tie(status.lowPrioOngoingRequests,
           status.lowPrioQueuedRequests)
      = SchedulerFeature::SCHEDULER->getNumberLowPrioOngoingAndQueued();

  return status;
}

}  // namespace arangodb
