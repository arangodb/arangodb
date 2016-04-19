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

#include "Supervision.h"

#include "Agent.h"
#include "Store.h"

#include "Basics/ConditionLocker.h"

using namespace arangodb::consensus;

Supervision::Supervision() : arangodb::Thread("Supervision"), _agent(nullptr) {}

Supervision::~Supervision() {
  shutdown();
};

void Supervision::wakeUp () {
  _cv.signal();
}

bool Supervision::doChecks (bool timedout) {
  LOG_TOPIC(INFO, Logger::AGENCY) << "Sanity checks";
  return true;
}

void Supervision::run() {

  CONDITION_LOCKER(guard, _cv);
  TRI_ASSERT(_agent!=nullptr);
  bool timedout = false;
  
  while (!this->isStopping()) {
    
    if (_agent->leading()) {
      timedout = _cv.wait(_frequency);//quarter second
    } else {
      _cv.wait();
    }
    
    doChecks(timedout);
    
  }
  
}

// Start thread
bool Supervision::start () {
  Thread::start();
  return true;
}

// Start thread with agent
bool Supervision::start (Agent* agent) {
  _agent = agent;
  _frequency = static_cast<long>(1.0e6*_agent->config().supervisionFrequency);
  return start();
}

void Supervision::beginShutdown() {
  // Personal hygiene
  Thread::beginShutdown();
}

Store const& Supervision::store() const {
  return _agent->readDB();
}
