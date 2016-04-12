#include "SanityCheck.h"
#include "Agent.h"

#include "Basics/ConditionLocker.h"

using namespace arangodb::consensus;

SanityCheck::SanityCheck() : arangodb::Thread("SanityCheck"), _agent(nullptr) {}

SanityCheck::~SanityCheck() {
  shutdown();
};

void SanityCheck::wakeUp () {
  _cv.signal();
}

bool SanityCheck::doChecks (bool timedout) {
  LOG_TOPIC(INFO, Logger::AGENCY) << "Sanity checks";
  return true;
}

void SanityCheck::run() {

  CONDITION_LOCKER(guard, _cv);
  TRI_ASSERT(_agent!=nullptr);
  bool timedout = false;
  
  while (!this->isStopping()) {
    
    if (_agent->leading()) {
      timedout = _cv.wait(1000000);
    } else {
      _cv.wait();
    }
    
    doChecks(timedout);
    
  }
  
}

// Start thread
bool SanityCheck::start () {
  Thread::start();
  return true;
}

// Start thread with agent
bool SanityCheck::start (Agent* agent) {
  _agent = agent;
  return start();
}

void SanityCheck::beginShutdown() {
  // Personal hygiene
  Thread::beginShutdown();
}
