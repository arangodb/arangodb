////////////////////////////////////////////////////////////////////////////////
/// @brief sharding configuration
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
/// @author Jan Steemann
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "ApplicationSharding.h"

using namespace triagens;
using namespace triagens::basics;
using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                         class ApplicationSharding
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

ApplicationSharding::ApplicationSharding () 
  : ApplicationFeature("Sharding"),
    _adminEndpoints(),
    _adminPrefix(),
    _myId(),
    _myAddress(),
    _enableSharding(false) {

}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

ApplicationSharding::~ApplicationSharding () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                        ApplicationFeature methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ApplicationSharding::setupOptions (map<string, basics::ProgramOptionsDescription>& options) {
  options["Sharding Options:help-sharding"]
    ("server.admin-endpoint", &_adminEndpoints, "admin endpoint to connect to")
    ("server.admin-prefix", &_adminPrefix, "admin prefix")
    ("server.my-id", &_myId, "this server's id")
    ("server.my-address", &_myAddress, "this server's endpoint")
    ("server.enable-sharding", &_enableSharding, "enable sharding")
  ;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationSharding::prepare () {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationSharding::start () {
  // TODO: start heartbeat
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ApplicationSharding::close () {
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ApplicationSharding::stop () {
  // TODO: stop heartbeat
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
