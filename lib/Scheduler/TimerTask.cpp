////////////////////////////////////////////////////////////////////////////////
/// @brief tasks used to handle timer events
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Achim Brandt
/// @author Copyright 2008-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "TimerTask.h"

#include "Logger/Logger.h"

#include "Scheduler/Scheduler.h"

using namespace triagens::basics;
using namespace triagens::rest;

// -----------------------------------------------------------------------------
// constructors and destructors
// -----------------------------------------------------------------------------

TimerTask::TimerTask (double seconds)
  : Task("TimerTask"),
    watcher(0),
    seconds(seconds) {
}



TimerTask::~TimerTask () {
}

// -----------------------------------------------------------------------------
// Task methods
// -----------------------------------------------------------------------------

bool TimerTask::setup (Scheduler* scheduler, EventLoop loop) {
  this->scheduler = scheduler;
  this->loop = loop;

  if (0.0 < seconds) {
    watcher = scheduler->installTimerEvent(loop, this, seconds);
    LOGGER_TRACE("armed TimerTask with " << seconds << " seconds");
  }
  else {
    watcher = 0;
  }
  return true;
}



void TimerTask::cleanup () {
  if (scheduler == 0) {
    LOGGER_WARNING("In TimerTask::cleanup the scheduler has disappeared -- invalid pointer");
    watcher = 0;
    return;
  }
  scheduler->uninstallEvent(watcher);
  watcher = 0;
}



bool TimerTask::handleEvent (EventToken token, EventType revents) {
  bool result = true;

  if (token == watcher) {
    if (revents & EVENT_TIMER) {
      scheduler->uninstallEvent(watcher);
      watcher = 0;

      result = handleTimeout();
    }
  }

  return result;
}
