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
    watcher(nullptr) {
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

  // will throw if it goes wrong...
  watcher = scheduler->installAsyncEvent(loop, this);

  return true;
}

void AsyncTask::cleanup () {
  if (_scheduler != nullptr) {
    if (watcher != nullptr) {
      _scheduler->uninstallEvent(watcher);
    }
  }

  watcher = nullptr;
}

bool AsyncTask::handleEvent (EventToken token, EventType revents) {
  if (watcher == token && (revents & EVENT_ASYNC)) {
    return handleAsync();
  }

  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
