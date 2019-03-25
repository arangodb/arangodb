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
  startsBefore("Database");
      
  _gcfunc = [this] (bool cancelled) {
    if (!cancelled) {
      MANAGER->garbageCollect(/*abortAll*/false);
    }
    
    if (!ApplicationServer::isStopping() && !cancelled) {
      auto off = std::chrono::seconds(1);
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
  _workItem = SchedulerFeature::SCHEDULER->queueDelay(RequestLane::INTERNAL_LOW, off, _gcfunc);
}
  
void ManagerFeature::beginShutdown() {
  _workItem.reset();
  // at this point all cursor should have been aborted already
  MANAGER->garbageCollect(/*abortAll*/true);
  // make sure no lingering managed trx remain
  while (MANAGER->garbageCollect(/*abortAll*/true)) {
    LOG_TOPIC("96298", WARN, Logger::TRANSACTIONS) << "still waiting for managed transaction";
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}

void ManagerFeature::unprepare() {
  MANAGER.reset();
}

}  // namespace transaction
}  // namespace arangodb
