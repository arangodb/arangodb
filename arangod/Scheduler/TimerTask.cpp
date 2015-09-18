////////////////////////////////////////////////////////////////////////////////
/// @brief tasks used to handle timer events
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
/// @author Copyright 2008-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "TimerTask.h"

#include "Basics/json.h"
#include "Basics/logging.h"
#include "Scheduler/Scheduler.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;

// -----------------------------------------------------------------------------
// constructors and destructors
// -----------------------------------------------------------------------------

TimerTask::TimerTask (std::string const& id,
                      double seconds)
  : Task(id, "TimerTask"),
    _watcher(nullptr),
    _seconds(seconds) {
}

TimerTask::~TimerTask () {
  cleanup();
}

// -----------------------------------------------------------------------------
// Task methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief get a task specific description in JSON format
////////////////////////////////////////////////////////////////////////////////

void TimerTask::getDescription (TRI_json_t* json) const {
  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, json, "type", TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, "timed", strlen("timed")));
  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, json, "offset", TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, _seconds));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set up task
////////////////////////////////////////////////////////////////////////////////

bool TimerTask::setup (Scheduler* scheduler, EventLoop loop) {
  _scheduler = scheduler;
  _loop = loop;

  if (0.0 < _seconds) {
    _watcher = _scheduler->installTimerEvent(loop, this, _seconds);
    LOG_TRACE("armed TimerTask with %f seconds", _seconds);
  }
  else {
    _watcher = nullptr;
  }

  return true;
}

void TimerTask::cleanup () {
  if (_scheduler != nullptr) {
    _scheduler->uninstallEvent(_watcher);
  }
  _watcher = nullptr;
}

bool TimerTask::handleEvent (EventToken token, 
                             EventType revents) {
  bool result = true;

  if (token == _watcher) {
    if (revents & EVENT_TIMER) {
      cleanup();
      result = handleTimeout();
    }
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
