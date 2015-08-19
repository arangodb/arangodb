////////////////////////////////////////////////////////////////////////////////
/// @brief requeue task
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
/// @author Copyright 2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "RequeueTask.h"

#include "Basics/logging.h"
#include "Dispatcher/Dispatcher.h"
#include "Scheduler/Scheduler.h"

using namespace std;
using namespace triagens::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

RequeueTask::RequeueTask (Scheduler* scheduler,
                          Dispatcher* dispatcher,
                          double sleep,
                          Job* job)
  : Task("Requeue Task"),
    TimerTask("Requeue Task", sleep),
    _scheduler(scheduler),
    _dispatcher(dispatcher),
    _job(job) {
}

// -----------------------------------------------------------------------------
// --SECTION--                                              PeriodicTask methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief handles the next tick
////////////////////////////////////////////////////////////////////////////////

bool RequeueTask::handleTimeout () {
  _dispatcher->addJob(_job);
  _scheduler->destroyTask(this);

  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
