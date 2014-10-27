////////////////////////////////////////////////////////////////////////////////
/// @brief manages open HTTP connections on the client side
///
/// @file ConnectionManager.cpp
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
/// @author Max Neunhoeffer
/// @author Copyright 2014, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/WriteLocker.h"

#include "ConnectionManager.h"

using namespace std;
using namespace triagens::httpclient;

////////////////////////////////////////////////////////////////////////////////
/// @brief global options for connections
////////////////////////////////////////////////////////////////////////////////

ConnectionOptions ConnectionManager::_globalConnectionOptions = {
  15.0,  // connectTimeout
  3.0,   // requestTimeout
  3,     // numRetries
  5.0,   // singleRequestTimeout
  0      // sslProtocol
};

////////////////////////////////////////////////////////////////////////////////
/// @brief the actual singleton instance
////////////////////////////////////////////////////////////////////////////////

ConnectionManager* ConnectionManager::_theinstance = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

ConnectionManager::~ConnectionManager ( ) {
  WRITE_LOCKER(allLock);
  map<string,ServerConnections*>::iterator i;
  for (i = allConnections.begin(); i != allConnections.end(); ++i) {
    delete i->second;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor for SingleServerConnection class
////////////////////////////////////////////////////////////////////////////////

ConnectionManager::SingleServerConnection::~SingleServerConnection () {
  delete connection;
  delete endpoint;
  lastUsed = 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor of ServerConnections class
////////////////////////////////////////////////////////////////////////////////

ConnectionManager::ServerConnections::~ServerConnections () {
  vector<SingleServerConnection*>::iterator i;
  WRITE_LOCKER(lock);

  unused.clear();
  for (i = connections.begin();i != connections.end();++i) {
    delete *i;
  }
  connections.clear();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief open or get a previously cached connection to a server
////////////////////////////////////////////////////////////////////////////////

ConnectionManager::SingleServerConnection*
ConnectionManager::leaseConnection (std::string& endpoint) {
  map<string,ServerConnections*>::iterator i;
  ServerConnections* s;
  SingleServerConnection* c;

  // First find a connections list:
  {
    WRITE_LOCKER(allLock);

    i = allConnections.find(endpoint);
    if (i != allConnections.end()) {
      s = i->second;
    }
    else {
      s = new ServerConnections();
      allConnections[endpoint] = s;
    }
  }

  TRI_ASSERT(s != nullptr);

  // Now get an unused one:
  {
    WRITE_LOCKER(s->lock);
    if (! s->unused.empty()) {
      c = s->unused.back();
      s->unused.pop_back();
      return c;
    }
  }

  triagens::rest::Endpoint* e
      = triagens::rest::Endpoint::clientFactory(endpoint);
  if (nullptr == e) {
    return nullptr;
  }
  triagens::httpclient::GeneralClientConnection*
         g = triagens::httpclient::GeneralClientConnection::factory(
                   e,
                   _globalConnectionOptions._requestTimeout,
                   _globalConnectionOptions._connectTimeout,
                   _globalConnectionOptions._connectRetries,
                   _globalConnectionOptions._sslProtocol);
  if (nullptr == g) {
    delete e;
    return nullptr;
  }
  if (! g->connect()) {
    delete g;
    delete e;
    return nullptr;
  }

  c = new SingleServerConnection(g, e, endpoint);
  if (nullptr == c) {
    delete g;
    delete e;
    return nullptr;
  }

  // Now put it into our administration:
  {
    WRITE_LOCKER(s->lock);
    s->connections.push_back(c);
  }
  c->lastUsed = time(0);
  return c;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return leased connection to a server
////////////////////////////////////////////////////////////////////////////////

void ConnectionManager::returnConnection (SingleServerConnection* c) {
  map<string,ServerConnections*>::iterator i;
  ServerConnections* s;

  if (! c->connection->isConnected()) {
    brokenConnection(c);
    return;
  }

  // First find the collections list:
  {
    WRITE_LOCKER(allLock);

    i = allConnections.find(c->ep_spec);
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

////////////////////////////////////////////////////////////////////////////////
/// @brief report a leased connection as being broken
////////////////////////////////////////////////////////////////////////////////

void ConnectionManager::brokenConnection (SingleServerConnection* c) {
  map<string,ServerConnections*>::iterator i;
  ServerConnections* s;

  // First find the collections list:
  {
    WRITE_LOCKER(allLock);

    i = allConnections.find(c->ep_spec);
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

////////////////////////////////////////////////////////////////////////////////
/// @brief closes all connections that have been unused for more than
/// limit seconds
////////////////////////////////////////////////////////////////////////////////

void ConnectionManager::closeUnusedConnections (double limit) {
  WRITE_LOCKER(allLock);
  map<string,ServerConnections*>::iterator s;
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
        if (t - (*i)->lastUsed > limit) {
          vector<SingleServerConnection*>::iterator j;
          for (j = sc->connections.begin(); j != sc->connections.end(); ++j) {
            if (*j == *i) {
              sc->connections.erase(j);
              break;
            }
          }
          delete (*i);
          sc->unused.erase(i);
          if (haveprev) {
            i = prev;   // will be incremented in next iteration
            ++i;
            haveprev = false;
          }
          else {
            i = sc->unused.begin();
          }
        }
        else {
          prev = i;
          ++i;
          haveprev = true;
        }
      }
    }
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
