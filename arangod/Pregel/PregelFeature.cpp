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
#include "Cluster/ClusterInfo.h"
#include "Pregel/Conductor.h"
#include "Pregel/Worker.h"
#include "Pregel/Recovery.h"
#include "Basics/MutexLocker.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Cluster/ClusterFeature.h"


using namespace arangodb::pregel;

static PregelFeature* Instance;

uint64_t PregelFeature::createExecutionNumber() {
  return ClusterInfo::instance()->uniqid();
}

PregelFeature::PregelFeature(application_features::ApplicationServer* server)
    : ApplicationFeature(server, "Pregel") {
  setOptional(false);
  requiresElevatedPrivileges(false);
  startsAfter("Database");
  startsAfter("Logger");
  startsAfter("Endpoint");
  startsAfter("Cluster");
  Instance = this;
}

PregelFeature::~PregelFeature() {
  if (_recoveryManager) {
    delete _recoveryManager;
  }
  cleanupAll();
}

PregelFeature* PregelFeature::instance() { return Instance; }

void PregelFeature::start() {
  ClusterFeature* cluster =
  application_features::ApplicationServer::getFeature<ClusterFeature>(
                                                                      "Cluster");
  if (cluster != nullptr) {
    AgencyCallbackRegistry *registry = cluster->agencyCallbackRegistry();
    if (registry != nullptr) {
      _recoveryManager = new RecoveryManager(registry);
    }
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

void PregelFeature::addWorker(IWorker* const worker,
                              uint64_t executionNumber) {
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
  if (wit != _workers.end()) {
    delete (wit->second);
    _workers.erase(executionNumber);
  }
}

void PregelFeature::cleanupAll() {
  MUTEX_LOCKER(guard, _mutex);
  for (auto it : _conductors) {
    delete (it.second);
  }
  _conductors.clear();
  for (auto it : _workers) {
    delete (it.second);
  }
  _workers.clear();
}

void PregelFeature::notifyConductors() {
  for (auto it : _conductors) {
    it.second->checkForWorkerOutage();
  }
}
