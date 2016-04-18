#include "Supervision.h"
#include "Agent.h"

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
      timedout = _cv.wait(250000);//quarter second
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
  return start();
}

void Supervision::beginShutdown() {
  // Personal hygiene
  Thread::beginShutdown();
}
