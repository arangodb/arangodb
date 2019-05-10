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
ConnectionPool::Ref ConnectionPool::leaseConnection(EndpointSpec const& str) {
  fuerte::ConnectionBuilder builder;
  builder.protocolType(_config.protocol);
  builder.endpoint(str);

  std::string endpoint = builder.normalizedEndpoint();
  
  READ_LOCKER(guard, _lock);
  auto it = _connections.find(endpoint);
  if (it == _connections.end()) {
    guard.unlock();
    WRITE_LOCKER(wguard, _lock);

    it = _connections.find(endpoint);  // check again
    if (it == _connections.end()) {
      auto it2 = _connections.emplace(endpoint, std::make_unique<ConnectionList>());
      it = it2.first;
    }
    return selectConnection(*(it->second), builder);
  }
  return selectConnection(*(it->second), builder);
}

/// @brief shutdown all connections
void ConnectionPool::shutdown() {
  WRITE_LOCKER(guard, _lock);
  for (auto& pair : _connections) {
    ConnectionList& list = *(pair.second);
    std::lock_guard<std::mutex> guard(list.mutex);
    for (auto& c : list.connections) {
      c->fuerte->cancel();
    }
  }
  _connections.clear();
}
  
void ConnectionPool::removeBrokenConnections(ConnectionList& list) {
  auto const now = std::chrono::steady_clock::now();
  auto it = list.connections.begin();
  while (it != list.connections.end()) {
    auto& c = *it;
    // lets not keep around diconnected fuerte connection objects
    auto lastUsed = now - c->lastUsed;
    if (c->fuerte->state() == fuerte::Connection::State::Failed ||
        (c->fuerte->state() == fuerte::Connection::State::Disconnected &&
         (lastUsed > std::chrono::seconds(5)))) {
          it = list.connections.erase(it);
    } else {
      it++;
    }
  }
}

/// remove unsued and broken connections
void ConnectionPool::pruneConnections() {
  READ_LOCKER(guard, _lock);

  const auto ttl = std::chrono::milliseconds(_config.connectionTtlMilli);
  for (auto& pair : _connections) {
    ConnectionList& list = *(pair.second);
    std::lock_guard<std::mutex> guard(list.mutex);

    auto now = std::chrono::steady_clock::now();

    removeBrokenConnections(list);

    // do not remove more connections than necessary
    if (list.connections.size() <= _config.minOpenConnections) {
      continue;
    }

    auto it = list.connections.begin();
    while (it != list.connections.end()) {
      Connection& c = *(it->get());

      size_t num = c.numLeased.load();
      auto lastUsed = now - c.lastUsed;
      TRI_ASSERT(lastUsed.count() >= 0);

      if (num == 0 && lastUsed > ttl) {
        it = list.connections.erase(it);
        // do not remove more connections than necessary
        if (list.connections.size() <= _config.minOpenConnections) {
          break;
        }
        continue;
      }

      if (num > 0) {  // continously update lastUsed
        c.lastUsed = now;
      }
      it++;
    }
    
    // do not remove connections if there are less
    if (list.connections.size() <= _config.maxOpenConnections) {
      continue; // done
    }
    
    it = list.connections.begin();
    while (it != list.connections.end()) {
      auto& c = *it;
      if (c->numLeased.load() == 0) {
        it = list.connections.erase(it);
        if (list.connections.size() <= _config.maxOpenConnections) {
          break;
        }
      }
      it++;
    }
  }
}
  
/// @brief cancel connections to this endpoint
void ConnectionPool::cancelConnections(EndpointSpec const& str) {
  fuerte::ConnectionBuilder builder;
  builder.protocolType(_config.protocol);
  builder.endpoint(str);
  
  std::string endpoint = builder.normalizedEndpoint();
  
  WRITE_LOCKER(guard, _lock);
  auto const& it = _connections.find(endpoint);
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
    ConnectionList& list = *(pair.second);
    std::lock_guard<std::mutex> guard(list.mutex);
    conns += list.connections.size();
  }
  return conns;
}

std::shared_ptr<fuerte::Connection> ConnectionPool::createConnection(fuerte::ConnectionBuilder& builder) {
  builder.timeout(std::chrono::milliseconds(_config.requestTimeoutMilli));
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

ConnectionPool::Ref ConnectionPool::selectConnection(ConnectionList& list,
                                                     fuerte::ConnectionBuilder& builder) {
  std::lock_guard<std::mutex> guard(list.mutex);

  for (auto& c : list.connections) {
    const auto state = c->fuerte->state();
    if (state == fuerte::Connection::State::Disconnected ||
        state == fuerte::Connection::State::Failed) {
      continue;
    }

    size_t num = c->numLeased.load(std::memory_order_acquire);
    // TODO: make configurable ?
    if ((builder.protocolType() == fuerte::ProtocolType::Http && num == 0) ||
        (builder.protocolType() == fuerte::ProtocolType::Vst && num < 4)) {
      c->numLeased.fetch_add(1);
      return Ref(c.get());
    }
  }

  list.connections.push_back(
      std::make_unique<ConnectionPool::Connection>(createConnection(builder)));
  return Ref(list.connections.back().get());
}

// =============== stupid reference counter ===============

ConnectionPool::Ref::Ref(ConnectionPool::Connection* c) : _conn(c) {
  if (_conn) {
    _conn->numLeased.fetch_add(1);
  }
}

ConnectionPool::Ref::Ref(Ref&& r) : _conn(std::move(r._conn)) {
  r._conn = nullptr;
}

ConnectionPool::Ref& ConnectionPool::Ref::operator=(Ref&& other) {
  if (_conn) {
    _conn->numLeased.fetch_sub(1);
  }
  _conn = other._conn;
  other._conn = nullptr;
  return *this;
}

ConnectionPool::Ref::Ref(Ref const& other) : _conn(other._conn) {
  if (_conn) {
    _conn->numLeased.fetch_add(1);
  }
};

ConnectionPool::Ref& ConnectionPool::Ref::operator=(Ref& other) {
  if (_conn) {
    _conn->numLeased.fetch_sub(1);
  }
  _conn = other._conn;
  if (_conn) {
    _conn->numLeased.fetch_add(1);
  }
  return *this;
}

ConnectionPool::Ref::~Ref() {
  if (_conn) {
    _conn->numLeased.fetch_sub(1);
  }
}

std::shared_ptr<fuerte::Connection> ConnectionPool::Ref::connection() const {
  return _conn->fuerte;
}

}  // namespace network
}  // namespace arangodb
