////////////////////////////////////////////////////////////////////////////////
/// @brief Class to get and cache information about the cluster state
///
/// @file ClusterState.cpp
///
/// DISCLAIMER
///
/// Copyright 2010-2013 triagens GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Copyright 2013, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Cluster/ClusterState.h"

#include "BasicsC/logging.h"
#include "Basics/WriteLocker.h"

using namespace triagens::arango;


// -----------------------------------------------------------------------------
// --SECTION--                                               ClusterState class
// -----------------------------------------------------------------------------

ClusterState* ClusterState::_theinstance = 0;

ClusterState* ClusterState::instance () {
  // This does not have to be thread-safe, because we guarantee that
  // this is called very early in the startup phase when there is still
  // a single thread.
  if (0 == _theinstance) {
    _theinstance = new ClusterState( );  // this now happens exactly once
  }
  return _theinstance;
}

void ClusterState::initialise () {
}

ClusterState::ClusterState ()
    : _agency() {
  loadServerInformation();
  loadShardInformation();
}

ClusterState::~ClusterState () {
}

void ClusterState::loadServerInformation () {
  while (true) {
    // tue die schmutzige Arbeit, verlasse mit return, sobald OK
    return;   // FIXME ...
    LOG_WARNING("ClusterState: Could not (re-)load agency data about servers");
  }
}

void ClusterState::loadShardInformation () {
  while (true) {
    // tue die schmutzige Arbeit, verlasse mit return, sobald OK
    return;   // FIXME ...
    LOG_WARNING("ClusterState: Could not (re-)load agency data about shards");
  }
}

std::string ClusterState::getServerEndpoint (ServerID& serverID) {
  map<ServerID,string>::iterator i = serverAddresses.find(serverID);
  if (i != serverAddresses.end()) {
    return i->second;
  }
  loadServerInformation();
  i = serverAddresses.find(serverID);
  if (i != serverAddresses.end()) {
    return i->second;
  }
  return string("");
}

ServerID ClusterState::getResponsibleServer (ShardID& shardID)
{
  map<ShardID,ServerID>::iterator i = shards.find(shardID);
  if (i != shards.end()) {
    return i->second;
  }
  loadServerInformation();
  i = shards.find(shardID);
  if (i != shards.end()) {
    return i->second;
  }
  return ServerID("");
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:


