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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#include "Compactor.h"

#include "Basics/ConditionLocker.h"
#include "Agency/Agent.h"


using namespace arangodb::consensus;


// @brief Construct with agent
Compactor::Compactor(Agent const* agent) :
  Thread("Compactor"), _agent(agent), _waitInterval(1000000) {
}


/// Dtor shuts down thread
Compactor::~Compactor() {
  //shutdown();
}


// @brief Run
void Compactor::run () {

  CONDITION_LOCKER(guard, _cv);

  while (!this->isStopping()) {
    _cv.wait(_waitInterval);
    Node node("compact");
    arangodb::consensus::index_t commitIndex = _agent->readDB(node);
    LOG(DEBUG) << "Compacting up to " << commitIndex;
    
  }
  
}


// @brief Wake up compaction
void Compactor::wakeUp () {
  {
    CONDITION_LOCKER(guard, _cv);
    guard.broadcast();
  }
}


// @brief Begin shutdown
void Compactor::beginShutdown() {

  Thread::beginShutdown();

  {
    CONDITION_LOCKER(guard, _cv);
    guard.broadcast();
  }
  
}
