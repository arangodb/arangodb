////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "ConnectionPool.h"

#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Logger/LogMacros.h"
#include "Network/NetworkFeature.h"
#include "Random/RandomGenerator.h"

#include <fuerte/connection.h>

namespace arangodb {
namespace network {

using namespace arangodb::fuerte::v1;

ConnectionPool::ConnectionPool(ConnectionPool::Config const& config)
    : _config(config), 
      _loop(config.numIOThreads, config.name) {
  TRI_ASSERT(config.numIOThreads > 0);
  TRI_ASSERT(_config.minOpenConnections <= _config.maxOpenConnections);
}

ConnectionPool::~ConnectionPool() { shutdownConnections(); }

/// @brief request a connection for a specific endpoint
/// note: it is the callers responsibility to ensure the endpoint
/// is always the same, we do not do any post-processing
network::ConnectionPtr ConnectionPool::leaseConnection(std::string const& endpoint) {
  READ_LOCKER(guard, _lock);
  auto it = _connections.find(endpoint);
  if (it == _connections.end()) {
    guard.unlock();

    auto tmp = std::make_unique<Bucket>(); //get memory outside lock
    
    WRITE_LOCKER(wguard, _lock);
    auto [it2, emplaced] = _connections.try_emplace(endpoint, std::move(tmp));
    it = it2;
  }
  return selectConnection(it->first,*(it->second));
}

/// @brief drain all connections
void ConnectionPool::drainConnections() {
  WRITE_LOCKER(guard, _lock);
  for (auto& pair : _connections) {
    Bucket& buck = *(pair.second);
    std::lock_guard<std::mutex> lock(buck.mutex);
    buck.list.clear();
  }
  _connections.clear();
}

/// @brief shutdown all connections
void ConnectionPool::shutdownConnections() {
  WRITE_LOCKER(guard, _lock);
  for (auto& pair : _connections) {
    Bucket& buck = *(pair.second);
    std::lock_guard<std::mutex> lock(buck.mutex);
    for (Context& c : buck.list) {
      c.fuerte->cancel();
    }
  }
}

/// remove unused and broken connections
void ConnectionPool::pruneConnections() {
  const auto ttl = std::chrono::milliseconds(_config.idleConnectionMilli * 2);

  READ_LOCKER(guard, _lock);
  for (auto& pair : _connections) {
    auto now = std::chrono::steady_clock::now();

    Bucket& buck = *(pair.second);
    std::lock_guard<std::mutex> lock(buck.mutex);

    // this loop removes broken connections, and closes the ones we don't
    // need anymore
    size_t aliveCount = 0;

    // make a single pass over the connections in this bucket
    auto it = buck.list.begin();
    while (it != buck.list.end()) {
      bool remove = false;

      if (it->fuerte->state() == fuerte::Connection::State::Failed) {
        // lets not keep around disconnected fuerte connection objects
        remove = true;
      } else {
        // connection has not yet failed
        if (it->fuerte->requestsLeft() > 0) {  // continuously update lastUsed
          it->leased = now;
          // connection will be kept
        } else if (aliveCount >= _config.minOpenConnections && 
                   (now - it->leased) > ttl) {
          // connection hasn't been used for a while, remove it
          // if we still have enough others
          remove = true;
        } else if (aliveCount >= _config.maxOpenConnections) {
          // remove superfluous connections
          remove = true;
        } // else keep the connection
      }

      if (remove) {
        it = buck.list.erase(it);
      } else {
        ++aliveCount;
        ++it;
        
        if (aliveCount == _config.maxOpenConnections && 
            it != buck.list.end()) {
          LOG_TOPIC("2d59a", DEBUG, Logger::COMMUNICATION)
            << "pruning extra connections to '" << pair.first
            << "' (" << buck.list.size() << ")";
        }
      }
    }
  }
}

/// @brief cancel connections to this endpoint
size_t ConnectionPool::cancelConnections(std::string const& endpoint) {
  WRITE_LOCKER(guard, _lock);
  auto const& it = _connections.find(endpoint);
  if (it != _connections.end()) {
    Bucket& buck = *(it->second);
    std::lock_guard<std::mutex> lock(buck.mutex);
    size_t n = buck.list.size();
    for (auto& c : buck.list) {
      c.fuerte->cancel();
    }
    _connections.erase(it);
    return n;
  }
  return 0;
}

/// @brief return the number of open connections
size_t ConnectionPool::numOpenConnections() const {
  size_t conns = 0;

  READ_LOCKER(guard, _lock);
  for (auto& pair : _connections) {
    Bucket& buck = *(pair.second);
    std::lock_guard<std::mutex> lock(buck.mutex);
    conns += buck.list.size();
  }
  return conns;
}

std::shared_ptr<fuerte::Connection> ConnectionPool::createConnection(fuerte::ConnectionBuilder& builder) {
  auto idle = std::chrono::milliseconds(_config.idleConnectionMilli);
  builder.idleTimeout(idle);
  builder.verifyHost(_config.verifyHosts);
  builder.protocolType(_config.protocol); // always overwrite protocol
  TRI_ASSERT(builder.socketType() != SocketType::Undefined);
    
  AuthenticationFeature* af = AuthenticationFeature::instance();
  if (af != nullptr && af->isActive()) {
    std::string const& token = af->tokenCache().jwtToken();
    if (!token.empty()) {
      builder.jwtToken(token);
      builder.authenticationType(fuerte::AuthenticationType::Jwt);
    }
  }
  //  builder.onFailure([this](fuerte::Error error,
  //                           const std::string& errorMessage) {
  //  });
  return builder.connect(_loop);
}

ConnectionPtr ConnectionPool::selectConnection(std::string const& endpoint,
                                               ConnectionPool::Bucket& bucket) {
  std::lock_guard<std::mutex> guard(bucket.mutex);

  for (Context& c : bucket.list) {
    const fuerte::Connection::State state = c.fuerte->state();
    if (state == fuerte::Connection::State::Failed) {
      continue;
    }
    
    TRI_ASSERT(_config.protocol != fuerte::ProtocolType::Undefined);

    size_t num = c.fuerte->requestsLeft();
    if (_config.protocol == fuerte::ProtocolType::Http && num == 0) {
      auto now = std::chrono::steady_clock::now();
      TRI_ASSERT(now >= c.leased);
      // hack hack hack. Avoids reusing used connections
      if ((now - c.leased) > std::chrono::milliseconds(25)) {
        c.leased = now;
        return c.fuerte;
      }
    } else if (_config.protocol == fuerte::ProtocolType::Vst && num <= 4) {
      c.leased = std::chrono::steady_clock::now();
      return c.fuerte; // TODO: make (num <= 4) configurable ?
    } else if (_config.protocol == fuerte::ProtocolType::Http2 && num <= 4) {
      c.leased = std::chrono::steady_clock::now();
      return c.fuerte; // TODO: make (num <= 4) configurable ?
    }
  }
  
  // no free connection found, so we add one
  LOG_TOPIC("2d6ab", DEBUG, Logger::COMMUNICATION) << "creating connection to "
    << endpoint << " bucket size  " << bucket.list.size();

  fuerte::ConnectionBuilder builder;
  builder.endpoint(endpoint); // picks the socket type

  std::shared_ptr<fuerte::Connection> fuerte = createConnection(builder);
  bucket.list.push_back(Context{fuerte, std::chrono::steady_clock::now()});
  return fuerte;
}

ConnectionPool::Config const& ConnectionPool::config() const { return _config; }

}  // namespace network
}  // namespace arangodb
