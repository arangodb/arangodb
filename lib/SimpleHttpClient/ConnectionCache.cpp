////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "ConnectionCache.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Exceptions.h"
#include "Basics/MutexLocker.h"
#include "Basics/debugging.h"
#include "Endpoint/Endpoint.h"
#include "Logger/LogMacros.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SslClientConnection.h"

namespace arangodb {
namespace httpclient {
  
ConnectionLease::ConnectionLease() 
    : _cache(nullptr), 
      _preventRecycling(false) {}

ConnectionLease::ConnectionLease(ConnectionCache* cache, 
                                 std::unique_ptr<GeneralClientConnection> connection)
    : _cache(cache),
      _connection(std::move(connection)),
      _preventRecycling(false) {}

ConnectionLease::~ConnectionLease() {
  if (_cache != nullptr && _connection != nullptr && !_preventRecycling) {
    _cache->release(std::move(_connection));
  }
}
  
ConnectionLease::ConnectionLease(ConnectionLease&& other) noexcept
    : _cache(other._cache),
      _connection(std::move(other._connection)),
      _preventRecycling(other._preventRecycling) {}
  
ConnectionLease& ConnectionLease::operator=(ConnectionLease&& other) noexcept {
  if (this != &other) {
    _cache = other._cache;
    _connection = std::move(other._connection);
    _preventRecycling = other._preventRecycling;
  }
  return *this;
}
  
void ConnectionLease::preventRecycling() noexcept {
  // this will prevent the connection from being inserted back into the connection cache
  _preventRecycling = true;
}

ConnectionCache::ConnectionCache(arangodb::application_features::ApplicationServer& server,
                                 Options const& options)
    : _server(server),
      _options(options),
      _connectionsCreated(0), 
      _connectionsRecycled(0) {}

ConnectionCache::~ConnectionCache() = default;

ConnectionLease ConnectionCache::acquire(std::string endpoint, 
                                         double connectTimeout,
                                         double requestTimeout,
                                         size_t connectRetries,
                                         uint64_t sslProtocol) {
  TRI_ASSERT(!endpoint.empty());

  // we must unify the endpoint here, because when we return the connection, we
  // will only have the unified form available
  endpoint = Endpoint::unifiedForm(endpoint);
        
  LOG_TOPIC("c869a", TRACE, Logger::REPLICATION) 
      << "trying to find connection for endpoint " << endpoint << " in connections cache";
  
  std::unique_ptr<GeneralClientConnection> connection;
  uint64_t metric;

  {
    MUTEX_LOCKER(locker, _lock);

    auto it = _connections.find(endpoint);
    if (it != _connections.end()) {
      auto& connectionsForEndpoint = (*it).second;

      for (auto it = connectionsForEndpoint.rbegin(); it != connectionsForEndpoint.rend(); ++it) {
        auto& candidate = (*it);
        if (candidate->getEndpoint()->encryption() != Endpoint::EncryptionType::NONE &&
            static_cast<SslClientConnection*>(candidate.get())->sslProtocol() != sslProtocol) {
          // different SSL protocol
          continue;
        }
       
        TRI_ASSERT(candidate->getEndpoint()->specification() == endpoint);

        // found a suitable candidate
        connection = std::move(candidate);
        TRI_ASSERT(connection != nullptr);

        if (it != connectionsForEndpoint.rbegin()) {
          // fill the gap
          (*it) = std::move(connectionsForEndpoint.back());
        }
        connectionsForEndpoint.pop_back();
        break;
      }
    }

    if (connection == nullptr) {
      metric = ++_connectionsCreated;
    } else {
      metric = ++_connectionsRecycled;
    }
  }
    
  // continue without the mutex
  
  if (connection == nullptr) {
    LOG_TOPIC("fd913", TRACE, Logger::REPLICATION) 
        << "did not find connection for endpoint " << endpoint << " in connections cache. creating new connection... created connections: " << metric;
    std::unique_ptr<Endpoint> ep(arangodb::Endpoint::clientFactory(endpoint));

    if (ep == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, std::string("unable to create endpoint '") + endpoint + "'");
    }
 
    // the unique_ptr ep is modified by the factory function and takes over ownership here
    connection.reset(GeneralClientConnection::factory(
        _server, ep, requestTimeout, connectTimeout, connectRetries, sslProtocol));

    TRI_ASSERT(connection != nullptr);
  } else {
    connection->repurpose(connectTimeout, requestTimeout, connectRetries);

    LOG_TOPIC("b8955", TRACE, Logger::REPLICATION) 
        << "found connection for endpoint " << endpoint << " in connections cache. recycled connections: " << metric;
  }

  return {this, std::move(connection)};
}

void ConnectionCache::release(std::unique_ptr<GeneralClientConnection> connection, bool force) try {
  if (connection == nullptr) {
    // nothing to do
    return;
  }

  if (connection->isConnected() || force) {
    std::string endpoint = connection->getEndpoint()->specification();
    TRI_ASSERT(!endpoint.empty());

    LOG_TOPIC("8e253", TRACE, Logger::REPLICATION) 
        << "putting connection for endpoint " << endpoint << " back into connections cache";

    MUTEX_LOCKER(locker, _lock);

    // this may create the vector at _connections[endpoint]
    auto& connectionsForEndpoint = _connections[endpoint];
    if (connectionsForEndpoint.size() < _options.maxConnectionsPerEndpoint) {
      connectionsForEndpoint.emplace_back(std::move(connection));
    }
  }
} catch (...) {
  // if we catch an exception here, the connection will be auto-destroyed, and no leaks will happen
}

}  // namespace httpclient
}  // namespace arangodb
