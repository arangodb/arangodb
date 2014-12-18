////////////////////////////////////////////////////////////////////////////////
/// @brief abstract base class for tasks
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
/// @author Copyright 2009-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Task.h"

#include "Basics/json.h"
#include "Scheduler/Scheduler.h"

using namespace triagens::rest;
using namespace std;

// -----------------------------------------------------------------------------
// constructors and destructors
// -----------------------------------------------------------------------------

Task::Task (string const& id,
            string const& name)
  : _scheduler(0),
    _loop(0),
    _id(id),
    _name(name),
    _active(1) {
}

Task::Task (string const& name)
  : _scheduler(0),
    _loop(0),
    _id(),
    _name(name),
    _active(1) {
}

Task::~Task () {
}

// -----------------------------------------------------------------------------
// public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief get a JSON representation of the task
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* Task::toJson () {
  TRI_json_t* json = TRI_CreateObjectJson(TRI_UNKNOWN_MEM_ZONE);

  if (json != 0) {
    TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, json, "id", TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, this->id().c_str()));
    TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, json, "name", TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, this->name().c_str()));

    this->getDescription(json);
  }

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the task is user-defined
/// note: this function may be overridden
////////////////////////////////////////////////////////////////////////////////

bool Task::isUserDefined () const {
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief allow thread to run on slave event loop
////////////////////////////////////////////////////////////////////////////////

bool Task::needsMainEventLoop () const {
  return false;
}

// -----------------------------------------------------------------------------
// protected methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief get a task specific description in JSON format
/// this does nothing for basic tasks, but derived classes may override it
////////////////////////////////////////////////////////////////////////////////

void Task::getDescription (TRI_json_t* json) {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
