////////////////////////////////////////////////////////////////////////////////
/// @brief Class to get and cache information about the cluster state
///
/// @file ClusterInfo.cpp
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

#include "Cluster/ClusterInfo.h"

#include "BasicsC/logging.h"
#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"

using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                                 ClusterInfo class
// -----------------------------------------------------------------------------

ClusterInfo* ClusterInfo::_theinstance = 0;

ClusterInfo* ClusterInfo::instance () {
  // This does not have to be thread-safe, because we guarantee that
  // this is called very early in the startup phase when there is still
  // a single thread.
  if (0 == _theinstance) {
    _theinstance = new ClusterInfo();  // this now happens exactly once
  }
  return _theinstance;
}

void ClusterInfo::initialise () {
}

ClusterInfo::ClusterInfo ()
  : _agency() {

  loadServerInformation();
  loadShardInformation();
}

ClusterInfo::~ClusterInfo () {
}

void ClusterInfo::loadServerInformation () {
  AgencyCommResult res;

  while (true) {
    {
      WRITE_LOCKER(lock); // TODO
      {
        AgencyCommLocker locker("Current", "READ");
        res = _agency.getValues("Current/ServersRegistered", true);
      }
      if (res.successful()) {
        if (res.flattenJson(serverAddresses,"Current/ServersRegistered/", false)) {
          LOG_TRACE("Current/ServersRegistered loaded successfully");
          //map<ServerID,string>::iterator i;
          //cout << "Servers registered:" << endl;
          //for (i = serverAddresses.begin(); i != serverAddresses.end(); ++i) {
          //  cout << "   " << i->first << " with address " << i->second 
          //       << endl;
          //}
          return;
        }
        else {
          LOG_DEBUG("Current/ServersRegistered not loaded successfully");
        }
      }
      else {
        LOG_DEBUG("Error whilst loading Current/ServersRegistered");
      }
    }

    usleep(100);
  }
}

std::string ClusterInfo::getServerEndpoint (ServerID const& serverID) {
  int tries = 0;

  while (++tries <= 2) {
    {
      READ_LOCKER(lock);
      map<ServerID,string>::iterator i = serverAddresses.find(serverID);
      if (i != serverAddresses.end()) {
        return i->second;
      }
    }
    
    // must call loadServerInformation outside the lock
    loadServerInformation();
  }

  return std::string("");
}

void ClusterInfo::loadShardInformation () {
  AgencyCommResult res;

  while (true) {
    {
      WRITE_LOCKER(lock);
      {
        AgencyCommLocker locker("Current", "READ");
        res = _agency.getValues("Current/ShardLocation", true);
      }
      if (res.successful()) {
        if (res.flattenJson(shards,"Current/ShardLocation/", false)) {
          LOG_TRACE("Current/ShardLocation loaded successfully");
          //map<ShardID,ServerID>::iterator i;
          //cout << "Shards:" << endl;
          //for (i = shards.begin(); i != shards.end(); ++i) {
          //  cout << "   " << i->first << " with responsible server " 
          //       << i->second << endl;
          //}
          return;
        }
        else {
          LOG_DEBUG("Current/ServersRegistered not loaded successfully");
        }
      }
      else {
        LOG_DEBUG("Error whilst loading Current/ServersRegistered");
      }
    }
    usleep(100);
  }
}

ServerID ClusterInfo::getResponsibleServer (ShardID const& shardID) {
  int tries = 0;

  while (++tries <= 2) {
    {
      READ_LOCKER(lock);
      map<ShardID,ServerID>::iterator i = shards.find(shardID);

      if (i != shards.end()) {
        return i->second;
      }
    }

    // must call loadShardInformation outside the lock
    loadShardInformation();
  }

  return ServerID("");
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
