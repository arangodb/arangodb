////////////////////////////////////////////////////////////////////////////////
/// @brief heartbeat thread
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

#include "HeartbeatThread.h"
#include "BasicsC/logging.h"

using namespace triagens::basics;
using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                                   HeartbeatThread
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a heartbeat thread
////////////////////////////////////////////////////////////////////////////////

HeartbeatThread::HeartbeatThread () 
  : Thread("heartbeat") {

  allowAsynchronousCancelation();

  _agency.connect();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a heartbeat thread
////////////////////////////////////////////////////////////////////////////////

HeartbeatThread::~HeartbeatThread () {
  _agency.disconnect();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    Thread methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief heartbeat main loop
////////////////////////////////////////////////////////////////////////////////

void HeartbeatThread::run () {
  LOG_INFO("starting heartbeat thread");

  while (1) {
    sleep(5);

    LOG_INFO("heartbeat");
  }
  
  LOG_INFO("stopping heartbeat thread");
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

