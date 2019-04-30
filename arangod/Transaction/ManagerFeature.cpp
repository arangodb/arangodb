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
#include "Basics/MutexLocker.h"
#include "Scheduler/SchedulerFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/Manager.h"

using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

namespace arangodb {
namespace transaction {

std::unique_ptr<transaction::Manager> ManagerFeature::MANAGER;

ManagerFeature::ManagerFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "TransactionManager"), _workItem(nullptr), _gcfunc() {
  setOptional(false);
  startsAfter("BasicsPhase");
  startsAfter("EngineSelector");
  startsAfter("Scheduler");
  startsBefore("Database");
      
  _gcfunc = [this] (bool canceled) {
    if (canceled) {
      return;
    }
    
    MANAGER->garbageCollect(/*abortAll*/false);
    
    auto off = std::chrono::seconds(1);
    
    std::lock_guard<std::mutex> guard(_workItemMutex);
    if (!ApplicationServer::isStopping() && !canceled) {
      _workItem = SchedulerFeature::SCHEDULER->queueDelay(RequestLane::INTERNAL_LOW, off, _gcfunc);
    }
  };
}

void ManagerFeature::prepare() {
  TRI_ASSERT(MANAGER == nullptr);
  TRI_ASSERT(EngineSelectorFeature::ENGINE != nullptr);
  MANAGER = EngineSelectorFeature::ENGINE->createTransactionManager();
}
  
void ManagerFeature::start() {
  auto off = std::chrono::seconds(1);

  Scheduler* scheduler = SchedulerFeature::SCHEDULER;
  if (scheduler != nullptr) {  // is nullptr in catch tests
    std::lock_guard<std::mutex> guard(_workItemMutex);
    _workItem = scheduler->queueDelay(RequestLane::INTERNAL_LOW, off, _gcfunc);
  }
}
  
void ManagerFeature::beginShutdown() {
  {
    std::lock_guard<std::mutex> guard(_workItemMutex);
    _workItem.reset();
  }
  // at this point all cursors should have been aborted already
  MANAGER->garbageCollect(/*abortAll*/true);
  // make sure no lingering managed trx remain
  while (MANAGER->garbageCollect(/*abortAll*/true)) {
    LOG_TOPIC("96298", WARN, Logger::TRANSACTIONS) << "still waiting for managed transaction";
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
