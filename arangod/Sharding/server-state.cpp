////////////////////////////////////////////////////////////////////////////////
/// @brief single-server states
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2013, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "server-state.h"

using namespace triagens::arango::serverstate;

// -----------------------------------------------------------------------------
// --SECTION--                                                     server states
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief get the string representation of a state
////////////////////////////////////////////////////////////////////////////////

std::string stateToString (StateEnum state) {
  switch (state) {
    case STATE_OFFLINE: 
      return "offline";
    case STATE_STARTUP:
      return "startup";
    case STATE_CONNECTED:
      return "connected";
    case STATE_STOPPING:
      return "stopping";
    case STATE_STOPPED:
      return "stopped";
    case STATE_PROBLEM:
      return "problem"; 
    case STATE_RECOVERING:
      return "recovering"; 
    case STATE_RECOVERED:
      return "recovered"; 
    case STATE_SHUTDOWN:
      return "shutdown"; 
  }

  assert(false);
  return "";
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

