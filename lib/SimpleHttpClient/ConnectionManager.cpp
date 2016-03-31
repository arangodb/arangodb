////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "ConnectionManager.h"
#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"

using namespace arangodb;
using namespace arangodb::httpclient;

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
/// @brief singleton instance of the connection manager
////////////////////////////////////////////////////////////////////////////////

static ConnectionManager* Instance = nullptr;

ConnectionManager::~ConnectionManager() {
  for (size_t i = 0; i < ConnectionManagerBuckets(); ++i) {
    WRITE_LOCKER(writeLocker, _connectionsBuckets[i]._lock);

    for (auto& it : _connectionsBuckets[i]._connections) {
      delete it.second;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize the connection manager
////////////////////////////////////////////////////////////////////////////////

void ConnectionManager::initialize() { Instance = new ConnectionManager(); }

////////////////////////////////////////////////////////////////////////////////
/// @brief get the unique instance
////////////////////////////////////////////////////////////////////////////////

ConnectionManager* ConnectionManager::instance() { return Instance; }

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor for SingleServerConnection class
////////////////////////////////////////////////////////////////////////////////

ConnectionManager::SingleServerConnection::~SingleServerConnection() {
  delete _connection;
  delete _endpoint;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor of ServerConnections class
////////////////////////////////////////////////////////////////////////////////

ConnectionManager::ServerConnections::~ServerConnections() {
  WRITE_LOCKER(writeLocker, _lock);

  for (auto& it : _connections) {
    delete it;
  }

  _connections.clear();
  _unused.clear();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a single connection
////////////////////////////////////////////////////////////////////////////////

void ConnectionManager::ServerConnections::addConnection(
    ConnectionManager::SingleServerConnection* connection) {
  WRITE_LOCKER(writeLocker, _lock);

  _connections.emplace_back(connection);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief pop a free connection - returns nullptr if no connection is
/// available
////////////////////////////////////////////////////////////////////////////////

ConnectionManager::SingleServerConnection*
ConnectionManager::ServerConnections::popConnection() {
  // get an unused connection
  {
    WRITE_LOCKER(writeLocker, _lock);

    if (!_unused.empty()) {
      auto connection = _unused.back();
      _unused.pop_back();

      return connection;
    }
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief push a unused connection back on the stack, allowing its re-use
////////////////////////////////////////////////////////////////////////////////

void ConnectionManager::ServerConnections::pushConnection(
    ConnectionManager::SingleServerConnection* connection) {
  connection->_lastUsed = time(0);

  WRITE_LOCKER(writeLocker, _lock);
  _unused.emplace_back(connection);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove a (broken) connection from the list of connections
////////////////////////////////////////////////////////////////////////////////

void ConnectionManager::ServerConnections::removeConnection(
    ConnectionManager::SingleServerConnection* connection) {
  WRITE_LOCKER(writeLocker, _lock);

  for (auto it = _connections.begin(); it != _connections.end(); ++it) {
    if ((*it) == connection) {
      // got it, now remove it
      _connections.erase(it);
      return;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes unused connections
////////////////////////////////////////////////////////////////////////////////

void ConnectionManager::ServerConnections::closeUnusedConnections(
    double limit) {
  time_t const t = time(0);

  std::list<ConnectionManager::SingleServerConnection*>::iterator current;

  WRITE_LOCKER(writeLocker, _lock);

  for (current = _unused.begin(); current != _unused.end(); /* no hoisting */) {
    SingleServerConnection* connection = (*current);

    if (t - connection->_lastUsed <= limit) {
      // connection is still valid
      ++current;
    } else {
      // connection is timed out

      // remove from list of connections
      for (auto it = _connections.begin(); it != _connections.end(); ++it) {
        if ((*it) == connection) {
          _connections.erase(it);
          break;
        }
      }

      delete connection;
      current = _unused.erase(current);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief open or get a previously cached connection to a server
////////////////////////////////////////////////////////////////////////////////

ConnectionManager::SingleServerConnection* ConnectionManager::leaseConnection(
    std::string const& endpoint) {
  // first find a connections list
  // this is optimized for the fact that we mostly have a connections
  // list for an endpoint already
  auto const slot = bucket(endpoint);

  ServerConnections* s = nullptr;
  {
    READ_LOCKER(readLocker, _connectionsBuckets[slot]._lock);

    auto it = _connectionsBuckets[slot]._connections.find(endpoint);

    if (it != _connectionsBuckets[slot]._connections.end()) {
      s = (*it).second;
    }
  }

  if (s == nullptr) {
    // do not yet have a connections list for this endpoint, so let's create
    // one!
    auto sc = std::make_unique<ServerConnections>();

    sc->_connections.reserve(16);

    // note that it is possible for a concurrent thread to have created
    // a list for the same endpoint. this case is handled below

    WRITE_LOCKER(writeLocker, _connectionsBuckets[slot]._lock);

    auto it =
        _connectionsBuckets[slot]._connections.emplace(endpoint, sc.get());

    if (!it.second) {
      // insert didn't work -> another thread has concurrently created a
      // list for the same endpoint
      // this means the unique_ptr can free the just created object and
      // we only need to lookup the connections list from the map
      auto it2 = _connectionsBuckets[slot]._connections.find(endpoint);

      // we must have a result!
      TRI_ASSERT(it2 != _connectionsBuckets[slot]._connections.end());
      s = (*it2).second;
    } else {
      // insert has worked. the map is now responsible for managing the
      // connections list memory
      s = sc.release();
    }
  }

  // when we get here, we must have found a collections list
  TRI_ASSERT(s != nullptr);

  // now get an unused one
  {
    auto connection = s->popConnection();

    if (connection != nullptr) {
      return connection;
    }
  }

  // create a new connection object

  // create an endpoint object
  std::unique_ptr<Endpoint> ep(Endpoint::clientFactory(endpoint));

  if (ep == nullptr) {
    // out memory
    return nullptr;
  }

  // create a connection object
  std::unique_ptr<GeneralClientConnection> cn(GeneralClientConnection::factory(
      ep.get(), _globalConnectionOptions._requestTimeout,
      _globalConnectionOptions._connectTimeout,
      _globalConnectionOptions._connectRetries,
      _globalConnectionOptions._sslProtocol));

  if (cn == nullptr) {
    // out of memory
    return nullptr;
  }

  if (!cn->connect()) {
    // could not connect
    return nullptr;
  }

  // finally create the SingleServerConnection
  auto c =
      std::make_unique<SingleServerConnection>(s, cn.get(), ep.get(), endpoint);

  // Now put it into our administration:
  s->addConnection(c.get());

  // now the ServerConnections list is responsible for the memory management!
  cn.release();
  ep.release();

  return c.release();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return leased connection to a server
////////////////////////////////////////////////////////////////////////////////

void ConnectionManager::returnConnection(SingleServerConnection* connection) {
  if (!connection->_connection->isConnected()) {
    brokenConnection(connection);
    return;
  }

  auto manager = connection->_connections;
  TRI_ASSERT(manager != nullptr);

  manager->pushConnection(connection);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief report a leased connection as being broken
////////////////////////////////////////////////////////////////////////////////

void ConnectionManager::brokenConnection(SingleServerConnection* connection) {
  auto manager = connection->_connections;
  TRI_ASSERT(manager != nullptr);

  manager->removeConnection(connection);

  delete connection;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes all connections that have been unused for more than
/// limit seconds
////////////////////////////////////////////////////////////////////////////////

void ConnectionManager::closeUnusedConnections(double limit) {
  // copy the list of ServerConnections first
  std::vector<ConnectionManager::ServerConnections*> copy;
  {
    for (size_t i = 0; i < ConnectionManagerBuckets(); ++i) {
      READ_LOCKER(readLocker, _connectionsBuckets[i]._lock);

      for (auto& it : _connectionsBuckets[i]._connections) {
        copy.emplace_back(it.second);
      }
    }
  }

  // now perform the cleanup for each ServerConnections object

  for (auto& it : copy) {
    it->closeUnusedConnections(limit);
  }
}
