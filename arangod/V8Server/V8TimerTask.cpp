////////////////////////////////////////////////////////////////////////////////
/// @brief timed V8 task
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

#include "V8TimerTask.h"

#include "BasicsC/json.h"
#include "Dispatcher/Dispatcher.h"
#include "Scheduler/Scheduler.h"
#include "V8/v8-conv.h"
#include "V8Server/V8Job.h"
#include "VocBase/server.h"

using namespace std;
using namespace triagens::rest;
using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

V8TimerTask::V8TimerTask (string const& id, 
                          string const& name,
                          TRI_vocbase_t* vocbase,
                          ApplicationV8* v8Dealer,
                          Scheduler* scheduler,
                          Dispatcher* dispatcher,
                          double offset,
                          string const& command,
                          TRI_json_t* parameters)
  : Task(id, name),
    TimerTask(id, offset),
    _vocbase(vocbase),
    _v8Dealer(v8Dealer),
    _dispatcher(dispatcher),
    _command(command),
    _parameters(parameters),
    _created(TRI_microtime()) {

  assert(vocbase != 0);

  // increase reference counter for the database used
  TRI_UseVocBase(_vocbase);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

V8TimerTask::~V8TimerTask () {
  // decrease reference counter for the database used
  TRI_ReleaseVocBase(_vocbase);

  if (_parameters != 0) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, _parameters);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 TimerTask methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief get a task specific description in JSON format
////////////////////////////////////////////////////////////////////////////////

void V8TimerTask::getDescription (TRI_json_t* json) {
  TimerTask::getDescription(json);

  TRI_json_t* created = TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, _created);
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "created", created);

  TRI_json_t* cmd = TRI_CreateString2CopyJson(TRI_UNKNOWN_MEM_ZONE, _command.c_str(), _command.size());
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "command", cmd);

  TRI_json_t* db = TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, _vocbase->_name);
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "database", db);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handles the timer event
////////////////////////////////////////////////////////////////////////////////

bool V8TimerTask::handleTimeout () {
  V8Job* job = new V8Job(
    _vocbase,
    _v8Dealer,
    "(function (params) { " + _command + " } )(params);",
    _parameters);
    
  _dispatcher->addJob(job);

  // note: this will destroy the task (i.e. ourselves!!)
  _scheduler->destroyTask(this);

  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
