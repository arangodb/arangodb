////////////////////////////////////////////////////////////////////////////////
/// @brief tasks used to handle periodic events
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2008-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "PeriodicTask.h"

#include "Scheduler/Scheduler.h"
#include "Logger/Logger.h"

using namespace triagens::rest;

// -----------------------------------------------------------------------------
// constructors and destructors
// -----------------------------------------------------------------------------

PeriodicTask::PeriodicTask (double offset, double interval)
  : Task("PeriodicTask"),
    watcher(0),
    offset(offset),
    interval(interval) {
}



PeriodicTask::~PeriodicTask () {
  if (watcher != 0) {
    scheduler->uninstallEvent(watcher);
  }
}

// -----------------------------------------------------------------------------
// Task methods
// -----------------------------------------------------------------------------

void PeriodicTask::resetTimer (double offset, double interval) {
  scheduler->rearmPeriodic(watcher, offset, interval);
}

// -----------------------------------------------------------------------------
// Task methods
// -----------------------------------------------------------------------------

bool PeriodicTask::setup (Scheduler* scheduler, EventLoop loop) {
  this->scheduler = scheduler;
  this->loop = loop;
  
  watcher = scheduler->installPeriodicEvent(loop, this, offset, interval);
  if (watcher == -1) {
    return false;
  }
  return true;
}



void PeriodicTask::cleanup () {
  if (scheduler == 0) {
    LOGGER_WARNING("In PeriodicTask::cleanup the scheduler has disappeared -- invalid pointer");
    watcher = 0;
    return;
  }
  scheduler->uninstallEvent(watcher);
  watcher = 0;
}



bool PeriodicTask::handleEvent (EventToken token, EventType revents) {
  bool result = true;
  
  if (token == watcher && (revents & EVENT_PERIODIC)) {
    result = handlePeriod();
  }
  
  return result;
}
