////////////////////////////////////////////////////////////////////////////////
/// @brief Library for intra-cluster communications
///
/// @file ClusterComm.cpp
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

#include "Cluster/ClusterComm.h"

#include "BasicsC/logging.h"
#include "Basics/WriteLocker.h"

#include "VocBase/server.h"

using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                   ClusterComm connection options 
// -----------------------------------------------------------------------------

ClusterCommOptions ClusterComm::_globalConnectionOptions = {
  15.0,  // connectTimeout 
  3.0,   // requestTimeout
  3      // numRetries
};


// -----------------------------------------------------------------------------
// --SECTION--                                                ClusterComm class
// -----------------------------------------------------------------------------

ClusterComm::ClusterComm ( ) {
  // FIXME: ...
}

ClusterComm* ClusterComm::_theinstance = 0;

ClusterComm* ClusterComm::instance () {
  // This does not have to be thread-safe, because we guarantee that
  // this is called very early in the startup phase when there is still
  // a single thread.
  if (0 == _theinstance) {
    _theinstance = new ClusterComm( );  // this now happens exactly once
  }
  return _theinstance;
}

OperationID ClusterComm::getOperationID () {
  return TRI_NewTickServer();
}

ClusterComm::SingleServerConnection::~SingleServerConnection () {
  delete connection;
  delete endpoint;
  lastUsed = 0;
  serverID = "";
}

ClusterComm::ServerConnections::~ServerConnections () {
  vector<SingleServerConnection*>::iterator i;
  WRITE_LOCKER(lock);

  unused.clear();
  for (i = connections.begin();i != connections.end();++i) {
    delete *i;
  }
  connections.clear();
}

ClusterComm::SingleServerConnection* 
ClusterComm::getConnection(ServerID& serverID) {
  map<ServerID,ServerConnections*>::iterator i;
  ServerConnections* s;
  SingleServerConnection* c;

  // First find a collections list:
  {
    WRITE_LOCKER(allLock);

    i = allConnections.find(serverID);
    if (i != allConnections.end()) {
      s = i->second;
    }
    else {
      s = new ServerConnections();
      allConnections[serverID] = s;
    }
  }
  
  // Now get an unused one:
  {
    WRITE_LOCKER(s->lock);
    if (!s->unused.empty()) {
      c = s->unused.back();
      s->unused.pop_back();
      c->lastUsed = time(0);
      return c;
    }
  }
  
  // We need to open a new one:
  string a = ClusterState::instance()->getServerEndpoint(serverID);
  if (a == "") {
    // Unknown server address, probably not yet connected
    return 0;
  }
  triagens::rest::Endpoint* e = triagens::rest::Endpoint::clientFactory(a);
  if (0 == e) {
    return 0;
  }
  triagens::httpclient::GeneralClientConnection*
         g = triagens::httpclient::GeneralClientConnection::factory(
                   e,
                   _globalConnectionOptions._requestTimeout,
                   _globalConnectionOptions._connectTimeout,
                   _globalConnectionOptions._connectRetries);
  if (0 == g) {
    delete e;
    return 0;
  }
  c = new SingleServerConnection(g,e,serverID);
  if (0 == c) {
    delete g;
    delete e;
    return 0;
  }

  // Now put it into our administration:
  {
    WRITE_LOCKER(s->lock);
    s->connections.push_back(c);
  }
  c->lastUsed = time(0);
  return c;
}


void ClusterComm::returnConnection(SingleServerConnection* c) {
  map<ServerID,ServerConnections*>::iterator i;
  ServerConnections* s;

  // First find the collections list:
  {
    WRITE_LOCKER(allLock);

    i = allConnections.find(c->serverID);
    if (i != allConnections.end()) {
      s = i->second;
    }
    else {
      // How strange! We just destroy the connection in despair!
      delete c;
      return;
    }
  }
  
  c->lastUsed = time(0);

  // Now mark it as unused:
  {
    WRITE_LOCKER(s->lock);
    s->unused.push_back(c);
  }
}

void ClusterComm::brokenConnection(SingleServerConnection* c) {
  map<ServerID,ServerConnections*>::iterator i;
  ServerConnections* s;

  // First find the collections list:
  {
    WRITE_LOCKER(allLock);

    i = allConnections.find(c->serverID);
    if (i != allConnections.end()) {
      s = i->second;
    }
    else {
      // How strange! We just destroy the connection in despair!
      delete c;
      return;
    }
  }
  
  // Now find it to get rid of it:
  {
    WRITE_LOCKER(s->lock);
    vector<SingleServerConnection*>::iterator i;
    for (i = s->connections.begin(); i != s->connections.end(); ++i) {
      if (*i == c) {
        // Got it, now remove it:
        s->connections.erase(i);
        delete c;
        return;
      }
    }
  }

  // How strange! We should have known this one!
  delete c;
}

void ClusterComm::closeUnusedConnections () {
  WRITE_LOCKER(allLock);
  map<ServerID,ServerConnections*>::iterator s;
  list<SingleServerConnection*>::iterator i;
  list<SingleServerConnection*>::iterator prev;
  ServerConnections* sc;
  time_t t;
  bool haveprev;

  t = time(0);
  for (s = allConnections.begin(); s != allConnections.end(); ++s) {
    sc = s->second;
    {
      WRITE_LOCKER(sc->lock);
      haveprev = false;
      for (i = sc->unused.begin(); i != sc->unused.end(); ) {
        if (t - (*i)->lastUsed > 120) {
          vector<SingleServerConnection*>::iterator j;
          for (j = sc->connections.begin(); j != sc->connections.end(); j++) {
            if (*j == *i) {
              sc->connections.erase(j);
              break;
            }
          }
          delete (*i);
          sc->unused.erase(i);
          if (haveprev) {
            i = prev;   // will be incremented in next iteration
            i++;
            haveprev = false;
          }
          else {
            i = sc->unused.begin();
          }
        }
        else {
          prev = i;
          i++;
          haveprev = true;
        }
      }
    }
  }
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:


