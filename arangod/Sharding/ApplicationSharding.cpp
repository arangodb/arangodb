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
#include "Rest/Endpoint.h"
#include "Sharding/HeartbeatThread.h"
#include "Sharding/ServerState.h"
#include "BasicsC/logging.h"

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
    _heartbeat(0),
    _agencyEndpoints(),
    _agencyPrefix(),
    _myId(),
    _myAddress(),
    _enableCluster(false) {

}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

ApplicationSharding::~ApplicationSharding () {
  if (_heartbeat != 0) {
    // flat line.....
    delete _heartbeat;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                        ApplicationFeature methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ApplicationSharding::setupOptions (map<string, basics::ProgramOptionsDescription>& options) {
  options["Sharding Options:help-sharding"]
    ("cluster.agency-endpoint", &_agencyEndpoints, "agency endpoint to connect to")
    ("cluster.agency-prefix", &_agencyPrefix, "agency prefix")
    ("cluster.my-id", &_myId, "this server's id")
    ("cluster.my-address", &_myAddress, "this server's endpoint")
  ;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationSharding::prepare () {
  _enableCluster = (_agencyEndpoints.size() > 0 || ! _agencyPrefix.empty());

  if (! enabled()) {
    return true;
  }

  // validate --cluster.agency-prefix
  size_t found = _agencyPrefix.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789/");

  if (found != std::string::npos) {
    LOG_FATAL_AND_EXIT("invalid value specified for --cluster.agency-prefix");
  }

  // register the prefix with the communicator
  AgencyComm::setPrefix(_agencyPrefix);

  
  // validate --cluster.agency-endpoint
  if (_agencyEndpoints.size() == 0) {
    LOG_FATAL_AND_EXIT("must at least specify one endpoint in --cluster.agency-endpoint");
  }

  for (size_t i = 0; i < _agencyEndpoints.size(); ++i) {
    const string unified = triagens::rest::Endpoint::getUnifiedForm(_agencyEndpoints[i]);

    if (unified.empty()) {
      LOG_FATAL_AND_EXIT("invalid endpoint '%s' specified for --cluster.agency-endpoint", _agencyEndpoints[i].c_str());
    }

    AgencyComm::addEndpoint(unified);
  }

  // validate --cluster.my-id
  if (_myId.empty()) {
    LOG_FATAL_AND_EXIT("invalid value specified for --cluster.my-id");
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationSharding::start () {
  if (! enabled()) {
    return true;
  }
  
  LOG_INFO("Clustering feature is turned on");

  ServerState::instance()->setCurrent(ServerState::STATE_STARTUP);

  _heartbeat = new HeartbeatThread(_myId);

  if (_heartbeat == 0) {
    LOG_FATAL_AND_EXIT("unable to start cluster heartbeat thread");
  }

  _heartbeat->start();

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ApplicationSharding::close () {
  if (! enabled()) {
    return;
  }
  
  _heartbeat->stop();
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ApplicationSharding::stop () {
  if (! enabled()) {
    return;
  }

  _heartbeat->stop();
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
