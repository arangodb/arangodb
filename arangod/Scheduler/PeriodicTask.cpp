////////////////////////////////////////////////////////////////////////////////
/// @brief tasks used to handle periodic events
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
/// @author Achim Brandt
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2008-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "PeriodicTask.h"

#include "Basics/json.h"
#include "Basics/logging.h"
#include "Scheduler/Scheduler.h"

using namespace std;
using namespace triagens::rest;

// -----------------------------------------------------------------------------
// constructors and destructors
// -----------------------------------------------------------------------------

PeriodicTask::PeriodicTask (string const& id,
                            double offset,
                            double interval)
  : Task(id, "PeriodicTask"),
    _watcher(nullptr),
    _offset(offset),
    _interval(interval) {
}

PeriodicTask::~PeriodicTask () {
  cleanup();
}

// -----------------------------------------------------------------------------
// Task methods
// -----------------------------------------------------------------------------

void PeriodicTask::resetTimer (double offset, double interval) {
  _scheduler->rearmPeriodic(_watcher, offset, interval);
}

// -----------------------------------------------------------------------------
// Task methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief get a task specific description in JSON format
////////////////////////////////////////////////////////////////////////////////

void PeriodicTask::getDescription (TRI_json_t* json) const {
  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, json, "type", TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, "periodic", strlen("periodic")));
  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, json, "period", TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, _interval));
}

bool PeriodicTask::setup (Scheduler* scheduler, EventLoop loop) {
  this->_scheduler = scheduler;
  this->_loop = loop;

  _watcher = scheduler->installPeriodicEvent(loop, this, _offset, _interval);

  return true;
}

void PeriodicTask::cleanup () {
  if (_scheduler != nullptr) {
    _scheduler->uninstallEvent(_watcher);
  }
  _watcher = nullptr;
}

bool PeriodicTask::handleEvent (EventToken token, 
                                EventType revents) {
  bool result = true;

  if (token == _watcher && (revents & EVENT_PERIODIC)) {
    result = handlePeriod();
  }

  return result;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
