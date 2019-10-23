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
#include "Network/NetworkFeature.h"
#include "Random/RandomGenerator.h"

#include <fuerte/connection.h>

namespace arangodb {
namespace network {

using namespace arangodb::fuerte::v1;

ConnectionPool::ConnectionPool(ConnectionPool::Config const& config)
    : _config(config), _loop(config.numIOThreads) {
      TRI_ASSERT(config.numIOThreads > 0);
      TRI_ASSERT(_config.minOpenConnections <= _config.maxOpenConnections);
    }

ConnectionPool::~ConnectionPool() { shutdown(); }

/// @brief request a connection for a specific endpoint
/// note: it is the callers responsibility to ensure the endpoint
/// is always the same, we do not do any post-processing
std::shared_ptr<network::Connection> ConnectionPool::leaseConnection(std::string const& endpoint) {

  READ_LOCKER(guard, _lock);
  auto it = _connections.find(endpoint);
  if (it == _connections.end()) {
    guard.unlock();
    WRITE_LOCKER(wguard, _lock);

    it = _connections.find(endpoint);  // check again
    if (it == _connections.end()) {
      auto it2 = _connections.emplace(endpoint, std::make_unique<Bucket>());
      it = it2.first;
    }
    return selectConnection(it->first, *(it->second));
  }
  return selectConnection(it->first,*(it->second));
}

/// @brief drain all connections
void ConnectionPool::drainConnections() {
  WRITE_LOCKER(guard, _lock);
  for (auto& pair : _connections) {
    Bucket& buck = *(pair.second);
    std::lock_guard<std::mutex> lock(buck.mutex);
    for (Context& c : buck.list) {
      c.fuerte->cancel();
    }
  }
}

/// @brief shutdown all connections
void ConnectionPool::shutdown() {
  WRITE_LOCKER(guard, _lock);
  _connections.clear();
}
  
void ConnectionPool::removeBrokenConnections(Bucket& buck) {
  auto it = buck.list.begin();
  while (it != buck.list.end()) {
    // lets not keep around diconnected fuerte connection objects
    if (it->fuerte->state() == fuerte::Connection::State::Failed) {
      it = buck.list.erase(it);
    } else {
      it++;
    }
  }
}

/// remove unsued and broken connections
void ConnectionPool::pruneConnections() {
  READ_LOCKER(guard, _lock);

  const auto ttl = std::chrono::milliseconds(_config.idleConnectionMilli);

  for (auto& pair : _connections) {
    Bucket& buck = *(pair.second);
    std::lock_guard<std::mutex> lock(buck.mutex);

    auto now = std::chrono::steady_clock::now();

    removeBrokenConnections(buck);

    // do not remove more connections than necessary
    if (buck.list.size() <= _config.minOpenConnections) {
      continue;
    }

    // first remove old connections
    auto it = buck.list.begin();
    while (it != buck.list.end()) {
      std::shared_ptr<fuerte::Connection> const& c = it->fuerte;

      if ((now - it->leased) > ttl) {
        it = buck.list.erase(it);
        // do not remove more connections than necessary
        if (buck.list.size() <= _config.minOpenConnections) {
          break;
        }
        continue;
      }

      if (c->requestsLeft() > 0) {  // continuously update lastUsed
        it->leased = now;
      }
      it++;
    }
    
    // do not remove connections if there are less
    if (buck.list.size() <= _config.maxOpenConnections) {
      continue; // done
    }
    
    // remove any remaining connections, they will be closed eventually
    it = buck.list.begin();
    while (it != buck.list.end()) {
      it = buck.list.erase(it);
      if (buck.list.size() <= _config.maxOpenConnections) {
        break;
      }
      continue;
    }
  }
}
  
/// @brief cancel connections to this endpoint
void ConnectionPool::cancelConnections(std::string const& endpoint) {
  fuerte::ConnectionBuilder builder;
  builder.endpoint(endpoint);
  builder.protocolType(_config.protocol); // always overwrite protocol
  
  std::string normalized = builder.normalizedEndpoint();
  
  WRITE_LOCKER(guard, _lock);
  auto const& it = _connections.find(normalized);
  if (it != _connections.end()) {
//    {
//      ConnectionList& list = *(it->second);
//      std::lock_guard<std::mutex> guard(list.mutex);
//      for (auto& c : list.connections) {
//        c->shutdown();
//      }
//    }
    _connections.erase(it);
  }
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
    
    size_t num = c.fuerte->requestsLeft();
    // TODO: make configurable ?
    if ((_config.protocol == fuerte::ProtocolType::Http && num == 0) ||
        (_config.protocol == fuerte::ProtocolType::Vst && num <= 4)) {
      return c.fuerte;
    }
  }
  
  fuerte::ConnectionBuilder builder;
  builder.endpoint(endpoint);
  builder.protocolType(_config.protocol); // always overwrite protocol
  
  std::shared_ptr<fuerte::Connection> fuerte = createConnection(builder);
  bucket.list.push_back(Context{fuerte, std::chrono::steady_clock::now()});
  return fuerte;
}

ConnectionPool::Config const& ConnectionPool::config() const { return _config; }

}  // namespace network
}  // namespace arangodb
