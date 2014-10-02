////////////////////////////////////////////////////////////////////////////////
/// @brief tasks used to handle asynchronous events
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

#include "AsyncTask.h"

#include "Basics/logging.h"
#include "Scheduler/Scheduler.h"

using namespace triagens::rest;

// -----------------------------------------------------------------------------
// constructors and destructors
// -----------------------------------------------------------------------------

AsyncTask::AsyncTask ()
  : Task("AsyncTask"),
    watcher(0) {
}



AsyncTask::~AsyncTask () {
}

// -----------------------------------------------------------------------------
// public methods
// -----------------------------------------------------------------------------

void AsyncTask::signal () {
  _scheduler->sendAsync(watcher);
}

// -----------------------------------------------------------------------------
// Task methods
// -----------------------------------------------------------------------------

bool AsyncTask::setup (Scheduler* scheduler,
                       EventLoop loop) {
  this->_scheduler = scheduler;
  this->_loop = loop;
  watcher = scheduler->installAsyncEvent(loop, this);
  if (watcher == -1) {
    return false;
  }
  return true;
}



void AsyncTask::cleanup () {
  if (_scheduler == 0) {
    LOG_WARNING("In AsyncTask::cleanup the scheduler has disappeared -- invalid pointer");
    watcher = 0;
    return;
  }
  _scheduler->uninstallEvent(watcher);
  watcher = 0;
}



bool AsyncTask::handleEvent (EventToken token, EventType revents) {
  bool result = true;

  if (watcher == token && (revents & EVENT_ASYNC)) {
    result = handleAsync();
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
