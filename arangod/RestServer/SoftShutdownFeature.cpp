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
#include "RestServer/ConsoleFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/ScriptFeature.h"
#include "RestServer/SoftShutdownFeature.h"
#include "Scheduler/SchedulerFeature.h"
#include "Transaction/Manager.h"
#include "Transaction/ManagerFeature.h"
#include "Utils/CursorRepository.h"
#include "VocBase/vocbase.h"

using namespace arangodb::options;

namespace {

bool queueShutdownChecker(std::mutex& mutex,
    arangodb::Scheduler::WorkHandle& workItem,
    std::function<void(bool)>& checkFunc) {

  arangodb::Scheduler* scheduler = arangodb::SchedulerFeature::SCHEDULER;
  bool queued = false;
  std::lock_guard<std::mutex> guard(mutex);
  std::tie(queued, workItem) =
      scheduler->queueDelay(arangodb::RequestLane::CLUSTER_INTERNAL,
                            std::chrono::seconds(2),
                            checkFunc);
  return queued;
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

SoftShutdownTracker::SoftShutdownTracker(
    application_features::ApplicationServer& server)
  : _server(server), _softShutdownOngoing(false) {
  _checkFunc = [this](bool cancelled) {
    if (!this->check()) {   // Initiates shutdown if counters are 0
      // Rearm ourselves:
      if (!::queueShutdownChecker(this->_workItemMutex, this->_workItem,
                                  this->_checkFunc)) {
        // If this does not work, shut down right away:
        this->initiateActualShutdown();
      }
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

  // Tell asynchronous job manager:
  auto& generalServerFeature = _server.getFeature<GeneralServerFeature>();
  auto& jobManager = generalServerFeature.jobManager();
  jobManager.initiateSoftShutdown();

  // And initiate our checker to watch numbers:
  if (!::queueShutdownChecker(_workItemMutex, _workItem, _checkFunc)) {
    // Make it hard in this case:
    LOG_TOPIC("de425", INFO, Logger::STARTUP)
        << "Failed to queue soft shutdown checker, doing hard shutdown "
           "instead.";
    std::this_thread::sleep_for(std::chrono::seconds(2));
    _server.beginShutdown();
  }
}
    
bool SoftShutdownTracker::check() const {
  Status status = getStatus();
  if (!status.allClear) {
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
  bool queued = scheduler->queue(RequestLane::CLUSTER_INTERNAL, [self] {
    // Give the server 2 seconds to finish stuff
    std::this_thread::sleep_for(std::chrono::seconds(2));
    self->_server.beginShutdown();
  });
  if (queued) {
    return;
  }
  std::this_thread::sleep_for(std::chrono::seconds(2));
  _server.beginShutdown();
}

void SoftShutdownTracker::toVelocyPack(VPackBuilder& builder,
    SoftShutdownTracker::Status const& status) {
  VPackObjectBuilder guard(&builder);
  builder.add("softShutdownOngoing", VPackValue(status.softShutdownOngoing));
  builder.add("AQLcursors", VPackValue(status.AQLcursors));
  builder.add("transactions", VPackValue(status.transactions));
  builder.add("pendingJobs", VPackValue(status.pendingJobs));
  builder.add("doneJobs", VPackValue(status.doneJobs));
  builder.add("allClear", VPackValue(status.allClear));
}

SoftShutdownTracker::Status SoftShutdownTracker::getStatus() const {
  Status status;
  status.softShutdownOngoing = _softShutdownOngoing.load(std::memory_order_relaxed);

  // Get number of active AQL cursors from each database:
  status.AQLcursors = 0;
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
  } else {
    status.transactions = 0;
  }

  // Get numbers of pending and done asynchronous jobs:
  auto& generalServerFeature = _server.getFeature<GeneralServerFeature>();
  auto& jobManager = generalServerFeature.jobManager();
  std::tie(status.pendingJobs, status.doneJobs)
      = jobManager.getNrPendingAndDone();

  status.allClear = status.AQLcursors == 0 &&
                    status.transactions == 0 &&
                    status.pendingJobs == 0 &&
                    status.doneJobs == 0;
  return status;
}

}  // namespace arangodb
