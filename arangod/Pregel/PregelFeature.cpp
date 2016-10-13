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
#include "Conductor.h"
#include "Worker.h"

using namespace arangodb::pregel;

static PregelFeature *Instance;

static unsigned int _exeI = 0;
unsigned int PregelFeature::createExecutionNumber() {
    return ++_exeI;
}

PregelFeature::PregelFeature(application_features::ApplicationServer* server)
: ApplicationFeature(server, "Pregel") {
    setOptional(true);
    requiresElevatedPrivileges(false);
    startsAfter("Database");
    startsAfter("Logger");
    startsAfter("Dispatcher");
    Instance = this;
}

PregelFeature::~PregelFeature() {
    cleanupAll();
}

PregelFeature* PregelFeature::instance() {
    return Instance;
}

void PregelFeature::beginShutdown() {
    cleanupAll();
}

void PregelFeature::addExecution(Conductor* const exec, unsigned int executionNumber) {
  //_executions.
  _conductors[executionNumber] = exec;
}

Conductor* PregelFeature::conductor(int executionNumber) {
  auto it = _conductors.find(executionNumber);
  if (it != _conductors.end()) return it->second;
  else return nullptr;
}

void PregelFeature::addWorker(Worker* const worker, unsigned int executionNumber) {
  _workers[executionNumber] = worker;
}

Worker* PregelFeature::worker(unsigned int executionNumber) {
  auto it = _workers.find(executionNumber);
  if (it != _workers.end()) return it->second;
  else return nullptr;
}

void PregelFeature::cleanup(unsigned int executionNumber) {
    auto cit = _conductors.find(executionNumber);
    if (cit != _conductors.end()) {
        delete(cit->second);
        _conductors.erase(executionNumber);
    }
    auto wit = _workers.find(executionNumber);
    if (wit != _workers.end()) {
        delete(wit->second);
        _workers.erase(executionNumber);
    }
}

void PregelFeature::cleanupAll() {
    for (auto it : _conductors) {
        delete(it.second);
    }
    _conductors.clear();
    for (auto it : _workers) {
        delete(it.second);
    }
    _workers.clear();
}
