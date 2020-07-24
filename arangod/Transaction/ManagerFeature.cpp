////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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

#include "ManagerFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/FunctionUtils.h"
#include "Basics/application-exit.h"
#include "FeaturePhases/BasicFeaturePhaseServer.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "RestServer/DatabaseFeature.h"
#include "Scheduler/SchedulerFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/Manager.h"

using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

namespace {
void queueGarbageCollection(std::mutex& mutex, arangodb::Scheduler::WorkHandle& workItem,
                            std::function<void(bool)>& gcfunc) {
  bool queued = false;
  {
    std::lock_guard<std::mutex> guard(mutex);
    std::tie(queued, workItem) =
        arangodb::basics::function_utils::retryUntilTimeout<arangodb::Scheduler::WorkHandle>(
            [&gcfunc]() -> std::pair<bool, arangodb::Scheduler::WorkHandle> {
              auto off = std::chrono::seconds(1);
              // The RequestLane needs to be something which is `HIGH` priority, otherwise
              // all threads executing this might be blocking, waiting for a lock to be
              // released.
              return arangodb::SchedulerFeature::SCHEDULER->queueDelay(arangodb::RequestLane::CLUSTER_INTERNAL,
                                                                       off, gcfunc);
            },
            arangodb::Logger::TRANSACTIONS,
            "queue transaction garbage collection");
  }
  if (!queued) {
    LOG_TOPIC("f8b3d", FATAL, arangodb::Logger::TRANSACTIONS)
        << "Failed to queue transaction garbage collection, for 5 minutes, "
           "exiting.";
    FATAL_ERROR_EXIT();
  }
}
}  // namespace

namespace arangodb {
namespace transaction {

std::unique_ptr<transaction::Manager> ManagerFeature::MANAGER;

ManagerFeature::ManagerFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "TransactionManager"),
      _workItem(nullptr),
      _gcfunc(),
      _streamingLockTimeout(8.0) {
  setOptional(false);
  startsAfter<BasicFeaturePhaseServer>();

  startsAfter<EngineSelectorFeature>();
  startsAfter<SchedulerFeature>();

  startsBefore<DatabaseFeature>();

  _gcfunc = [this] (bool canceled) {
    if (canceled) {
      return;
    }
    
    MANAGER->garbageCollect(/*abortAll*/false);

    if (!this->server().isStopping()) {
      ::queueGarbageCollection(_workItemMutex, _workItem, _gcfunc);
    }
  };
}

void ManagerFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("transaction", "Transaction features");

  options->addOption("--transaction.streaming-lock-timeout", "lock timeout in seconds "
		     "in case of parallel access to the same streaming transaction",
                     new DoubleParameter(&_streamingLockTimeout),
		     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden))
    .setIntroducedIn(30605).setIntroducedIn(30701);
}

void ManagerFeature::prepare() {
  TRI_ASSERT(MANAGER.get() == nullptr);
  TRI_ASSERT(server().getFeature<EngineSelectorFeature>().selected());
  MANAGER = server().getFeature<EngineSelectorFeature>().engine().createTransactionManager(*this);
}
  
void ManagerFeature::start() {
  Scheduler* scheduler = SchedulerFeature::SCHEDULER;
  if (scheduler != nullptr) {  // is nullptr in catch tests
    ::queueGarbageCollection(_workItemMutex, _workItem, _gcfunc);
  }
}
  
void ManagerFeature::beginShutdown() {
  {
    // when we get here, ApplicationServer::isStopping() will always return
    // true already. So it is ok to wait here until the workItem has been
    // fully canceled. We are grabbing the mutex here, so the workItem cannot
    // reschedule itself if it doesn't have the mutex. If it is executed
    // directly afterwards, it will check isStopping(), which will return
    // false, so no rescheduled will be performed
    // if it doesn't hold the mutex, we will cancel it here (under the mutex)
    // and when the callback is executed, it will check isStopping(), which 
    // will always return false
    std::lock_guard<std::mutex> guard(_workItemMutex);
    _workItem.reset();
  }

  MANAGER->disallowInserts();
  // at this point all cursors should have been aborted already
  MANAGER->garbageCollect(/*abortAll*/true);
  // make sure no lingering managed trx remain
  while (MANAGER->garbageCollect(/*abortAll*/true)) {
    LOG_TOPIC("96298", INFO, Logger::TRANSACTIONS) << "still waiting for managed transaction";
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}

void ManagerFeature::stop() {
  // reset again, as there may be a race between beginShutdown and
  // the execution of the deferred _workItem
  {
    std::lock_guard<std::mutex> guard(_workItemMutex);
    _workItem.reset();
  }

  // at this point all cursors should have been aborted already
  MANAGER->garbageCollect(/*abortAll*/true);
}

void ManagerFeature::unprepare() {
  MANAGER.reset();
}

}  // namespace transaction
}  // namespace arangodb
