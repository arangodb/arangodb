////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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

#include "Agency/Agent.h"
#include "Basics/ConditionLocker.h"
#include "Logger/LogMacros.h"

using namespace arangodb::consensus;

// @brief Construct with agent
Compactor::Compactor(Agent* agent)
    : Thread(agent->server(), "Compactor"),
      _agent(agent),
      _wakeupCompactor(false),
      _waitInterval(1000000) {}

/// Dtor shuts down thread
Compactor::~Compactor() {
  shutdown();
}

// @brief Run
void Compactor::run() {
  LOG_TOPIC("8fd95", DEBUG, Logger::AGENCY) << "Starting compactor personality";

  while (true) {
    bool falseAlarm = true;
    {
      CONDITION_LOCKER(guard, _cv);
      if (!_wakeupCompactor) {
        _cv.wait(5000000);  // just in case we miss a wakeup call!
      }
      if (_wakeupCompactor) {
        falseAlarm = false;
      }
      _wakeupCompactor = false;
    }

    if (this->isStopping()) {
      break;
    }

    try {
      if (!falseAlarm) {
        _agent->compact();  // Note that this checks nextCompactionAfter again!
      }
    } catch (std::exception const& e) {
      LOG_TOPIC("f9493", ERR, Logger::AGENCY)
          << "Exception during compaction, details: " << e.what();
    }
  }
}

// @brief Wake up compaction
void Compactor::wakeUp() {
  CONDITION_LOCKER(guard, _cv);
  _wakeupCompactor = true;
  _cv.signal();
}

// @brief Begin shutdown
void Compactor::beginShutdown() {
  LOG_TOPIC("4bad9", DEBUG, Logger::AGENCY) << "Shutting down compactor personality";

  Thread::beginShutdown();

  wakeUp();
}
