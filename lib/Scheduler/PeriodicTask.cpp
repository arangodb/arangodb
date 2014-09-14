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
    watcher(0),
    offset(offset),
    interval(interval) {
}

PeriodicTask::~PeriodicTask () {
  if (watcher != 0) {
    _scheduler->uninstallEvent(watcher);
  }
}

// -----------------------------------------------------------------------------
// Task methods
// -----------------------------------------------------------------------------

void PeriodicTask::resetTimer (double offset, double interval) {
  _scheduler->rearmPeriodic(watcher, offset, interval);
}

// -----------------------------------------------------------------------------
// Task methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief get a task specific description in JSON format
////////////////////////////////////////////////////////////////////////////////

void PeriodicTask::getDescription (TRI_json_t* json) {
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "type", TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, "periodic"));
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "period", TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, interval));
}

bool PeriodicTask::setup (Scheduler* scheduler, EventLoop loop) {
  this->_scheduler = scheduler;
  this->_loop = loop;

  watcher = scheduler->installPeriodicEvent(loop, this, offset, interval);

  if (watcher == -1) {
    return false;
  }
  return true;
}

void PeriodicTask::cleanup () {
  if (_scheduler == 0) {
    LOG_WARNING("In PeriodicTask::cleanup the scheduler has disappeared -- invalid pointer");
    watcher = 0;
    return;
  }
  _scheduler->uninstallEvent(watcher);
  watcher = 0;
}

bool PeriodicTask::handleEvent (EventToken token, EventType revents) {
  bool result = true;

  if (token == watcher && (revents & EVENT_PERIODIC)) {
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
