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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "PregelFeature.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/MutexLocker.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Pregel/AlgoRegistry.h"
#include "Pregel/Conductor.h"
#include "Pregel/Recovery.h"
#include "Pregel/ThreadPool.h"
#include "Pregel/Utils.h"
#include "Pregel/Worker.h"

using namespace arangodb;
using namespace arangodb::pregel;

static PregelFeature* Instance = nullptr;
static uint64_t _uniqueId = 0;
uint64_t PregelFeature::createExecutionNumber() {
  if (ServerState::instance()->isRunningInCluster()) {
    return ClusterInfo::instance()->uniqid();
  } else {
    return ++_uniqueId;
  }
}

PregelFeature::PregelFeature(application_features::ApplicationServer* server)
: application_features::ApplicationFeature(server, "Pregel") {
  setOptional(true);
  requiresElevatedPrivileges(false);
  startsAfter("Logger");
  startsAfter("Database");
  startsAfter("Endpoint");
  startsAfter("Cluster");
  startsAfter("Server");
  startsAfter("V8Dealer");
}

PregelFeature::~PregelFeature() {
  if (_recoveryManager) {
    _recoveryManager.reset();
  }
  cleanupAll();
}

PregelFeature* PregelFeature::instance() { return Instance; }

size_t PregelFeature::availableParallelism() {
  const size_t procNum = TRI_numberProcessors();
  return procNum < 1 ? 1 : procNum;
}

void PregelFeature::start() {
  Instance = this;
  if (ServerState::instance()->isAgent()) {
    return;
  }

  const size_t threadNum = PregelFeature::availableParallelism();
  LOG_TOPIC(INFO, Logger::PREGEL) << "Pregel uses " << threadNum << " threads";
  _threadPool.reset(new ThreadPool(threadNum, "Pregel"));

  if (ServerState::instance()->isCoordinator()) {
    _recoveryManager.reset(new RecoveryManager());
    //    ClusterFeature* cluster =
    //    application_features::ApplicationServer::getFeature<ClusterFeature>(
    //                                                                        "Cluster");
    //    if (cluster != nullptr) {
    //      AgencyCallbackRegistry* registry =
    //      cluster->agencyCallbackRegistry();
    //      if (registry != nullptr) {
    //        _recoveryManager.reset(new RecoveryManager(registry));
    //      }
    //    }
  }
}

void PregelFeature::beginShutdown() { cleanupAll(); }

void PregelFeature::addExecution(Conductor* const exec,
                                 uint64_t executionNumber) {
  MUTEX_LOCKER(guard, _mutex);
  //_executions.
  _conductors[executionNumber] = exec;
}

Conductor* PregelFeature::conductor(uint64_t executionNumber) {
  MUTEX_LOCKER(guard, _mutex);
  auto it = _conductors.find(executionNumber);
  return it != _conductors.end() ? it->second : nullptr;
}

void PregelFeature::addWorker(IWorker* const worker, uint64_t executionNumber) {
  MUTEX_LOCKER(guard, _mutex);
  _workers[executionNumber] = worker;
}

IWorker* PregelFeature::worker(uint64_t executionNumber) {
  MUTEX_LOCKER(guard, _mutex);
  auto it = _workers.find(executionNumber);
  return it != _workers.end() ? it->second : nullptr;
}

void PregelFeature::cleanup(uint64_t executionNumber) {
  MUTEX_LOCKER(guard, _mutex);
  auto cit = _conductors.find(executionNumber);
  if (cit != _conductors.end()) {
    delete (cit->second);
    _conductors.erase(executionNumber);
  }
  auto wit = _workers.find(executionNumber);
  if (wit != _workers.end()) {// unmapping etc might need time
    _threadPool->enqueue([this, executionNumber]{
        auto wit = _workers.find(executionNumber);
        delete (wit->second);
        _workers.erase(executionNumber);
    });
  }
}

void PregelFeature::cleanupAll() {
  MUTEX_LOCKER(guard, _mutex);
  for (auto it : _conductors) {
    delete (it.second);
  }
  _conductors.clear();
  for (auto it : _workers) {
    it.second->cancelGlobalStep(VPackSlice());
    usleep(1000 * 25);
    delete (it.second);
  }
  _workers.clear();
}

void PregelFeature::handleConductorRequest(std::string const& path,
                                           VPackSlice const& body,
                                           VPackBuilder& outBuilder) {
  VPackSlice sExecutionNum = body.get(Utils::executionNumberKey);
  if (!sExecutionNum.isInteger()) {
    LOG_TOPIC(ERR, Logger::PREGEL) << "Invalid execution number";
  }
  uint64_t executionNumber = sExecutionNum.getUInt();
  Conductor* co = Instance->conductor(executionNumber);
  if (!co) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "Conductor not found, invalid execution number");
  }

  if (path == Utils::finishedStartupPath) {
    co->finishedWorkerStartup(body);
  } else if (path == Utils::finishedWorkerStepPath) {
    outBuilder = co->finishedWorkerStep(body);
  } else if (path == Utils::finishedRecoveryPath) {
    co->finishedRecoveryStep(body);
  }
}

void PregelFeature::handleWorkerRequest(TRI_vocbase_t* vocbase,
                                        std::string const& path,
                                        VPackSlice const& body,
                                        VPackBuilder& outBuilder) {
  VPackSlice sExecutionNum = body.get(Utils::executionNumberKey);
  if (!sExecutionNum.isInteger()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "Worker not found, invalid execution number");
  }
  uint64_t executionNumber = sExecutionNum.getUInt();
  IWorker* w = PregelFeature::instance()->worker(executionNumber);

  // create a new worker instance if necessary
  if (path == Utils::startExecutionPath || path == Utils::startRecoveryPath) {
    if (w == nullptr) {
      w = AlgoRegistry::createWorker(vocbase, body);
      Instance->addWorker(w, executionNumber);
    } else if (path == Utils::startExecutionPath) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL,
          "Worker with this execution number already exists.");
    }
    if (path == Utils::startRecoveryPath) {
      w->startRecovery(body);
    }
  }
  if (w == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        "Worker with this execution number already exists.");
  }

  if (path == Utils::prepareGSSPath) {
    outBuilder = w->prepareGlobalStep(body);
  } else if (path == Utils::startGSSPath) {
    w->startGlobalStep(body);
  } else if (path == Utils::messagesPath) {
    w->receivedMessages(body);
  } else if (path == Utils::cancelGSSPath) {
    w->cancelGlobalStep(body);
  } else if (path == Utils::finalizeExecutionPath) {
    w->finalizeExecution(body);
    Instance->cleanup(executionNumber);
  } else if (path == Utils::continueRecoveryPath) {
    w->compensateStep(body);
  } else if (path == Utils::finalizeRecoveryPath) {
    w->finalizeRecovery(body);
  } else if (path == Utils::aqlResultsPath) {
    w->aqlResult(&outBuilder);
  }
}
