////////////////////////////////////////////////////////////////////////////////
/// @brief periodic V8 task
///
/// @file
///
/// DISCLAIMER
///
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "V8PeriodicTask.h"

#include "Dispatcher/Dispatcher.h"
#include "Scheduler/Scheduler.h"
#include "V8Server/V8PeriodicJob.h"

using namespace std;
using namespace triagens::rest;
using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

V8PeriodicTask::V8PeriodicTask (TRI_vocbase_t* vocbase,
                                ApplicationV8* v8Dealer,
                                Scheduler* scheduler,
                                Dispatcher* dispatcher,
                                double offset,
                                double period,
                                const string& module,
                                const string& func,
                                const string& parameter)
  : Task("V8 Periodic Task"),
    PeriodicTask(offset, period),
    _vocbase(vocbase),
    _v8Dealer(v8Dealer),
    _dispatcher(dispatcher),
    _module(module),
    _func(func),
    _parameter(parameter) {
}

// -----------------------------------------------------------------------------
// --SECTION--                                              PeriodicTask methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief handles the next tick
////////////////////////////////////////////////////////////////////////////////

bool V8PeriodicTask::handlePeriod () {
  V8PeriodicJob* job = new V8PeriodicJob(
    _vocbase,
    _v8Dealer,
    _module,
    _func,
    _parameter);

  _dispatcher->addJob(job);

  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
