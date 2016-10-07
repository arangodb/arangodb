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

#include "JobMapping.h"
#include "Conductor.h"
#include "Worker.h"

using namespace arangodb::pregel;

static int _exeI = 0;
int JobMapping::createExecutionNumber() {
  return ++_exeI;
}

void JobMapping::addExecution(Conductor* const exec, int executionNumber) {
  //_executions.
  _conductors[executionNumber] = exec;
}

Conductor* JobMapping::conductor(int executionNumber) {
  auto it = _conductors.find(executionNumber);
  if (it != _conductors.end()) return it->second;
  else return nullptr;
}

void JobMapping::addWorker(Worker* const worker, int executionNumber) {
  _workers[executionNumber] = worker;
}

Worker* JobMapping::worker(int executionNumber) {
  auto it = _workers.find(executionNumber);
  if (it != _workers.end()) return it->second;
  else return nullptr;
}
