////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

#include <fuerte/Connection.h>

namespace arangodb { namespace network {

using namespace arangodb::fuerte::v1;
  
ConnectionPool::ConnectionPool(NetworkFeature* net)
  : _network(net), _loop(net->numThreads()) {
}
  
/// @brief request a connection for a specific endpoint
/// note: it is the callers responsibility to ensure the endpoint
/// is always the same, we do not do any post-processing
  ConnectionPool::Ref ConnectionPool::leaseConnection(EndpointSpec str) {
  fuerte::ConnectionBuilder builder;
  builder.endpoint(str);
  
  str = builder.normalizedEndpoint();
  READ_LOCKER(guard, _lock);
  
  auto it = _connections.find(str);
  if (it == _connections.end()) {
    guard.unlock();
    WRITE_LOCKER(wguard, _lock);
    
    it = _connections.find(str); // check again
    if (it == _connections.end()) {
      auto it2 = _connections.emplace(str, std::make_unique<ConnectionList>());
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
    for (auto it : list.connections) {
      it.fuerte->cancel();
    }
  }
}
  
void ConnectionPool::pruneConnections() {
  READ_LOCKER(guard, _lock);
  
  const auto ttl = std::chrono::milliseconds(_network->connectionTtlMilli());
  
  for (auto& pair : _connections) {
    ConnectionList& list = *(pair.second);
    std::lock_guard<std::mutex> guard(list.mutex);

    auto it = list.connections.begin();
    while (it != list.connections.end()) {
      size_t num = it->numLeased.load();
      auto now = std::chrono::steady_clock::now();
      if (num > 0) {
        it->lastUsed = now;
        it++;
      } else if ( now - it->lastUsed < ttl) {
        it = list.connections.erase(it);
      }
    }
  }
}
  
std::shared_ptr<fuerte::Connection> ConnectionPool::createConnection(fuerte::ConnectionBuilder& builder) {
  builder.timeout(std::chrono::milliseconds(_network->requestTimeoutMilli()));
  AuthenticationFeature* af = AuthenticationFeature::instance();
  TRI_ASSERT(af != nullptr);
  if (af->isActive()) {
    std::string const& token = af->tokenCache().jwtToken();
    if (!token.empty()) {
      builder.jwtToken(token);
      builder.authenticationType(fuerte::AuthenticationType::Jwt);
    }
  }
//  builder.onFailure([this](fuerte::Error error) {
//
//  });
  
  return builder.connect(_loop);
}

  
ConnectionPool::Ref ConnectionPool::selectConnection(ConnectionList& list,
                                                     fuerte::ConnectionBuilder& builder) {
  std::lock_guard<std::mutex> guard(list.mutex);
  
  for (Connection& c : list.connections) {
    const auto state = c.fuerte->state();
    if (state == fuerte::Connection::State::Disconnected ||
        state == fuerte::Connection::State::Failed) {
      continue;
    }
    
    size_t num = c.numLeased.load(std::memory_order_acquire);
    if (builder.protocolType() == fuerte::ProtocolType::Http && num == 0) {
      c.numLeased.store(1);
      return Ref(&c);
    } else if (builder.protocolType() == fuerte::ProtocolType::Vst && num < 4) {
      c.numLeased.fetch_add(1);
      return Ref(&c);
    }
  }
  
  list.connections.emplace_back(createConnection(builder));
  list.connections.back().numLeased.store(1);
  return Ref(&list.connections.back());
}
  
// stupid reference counter
  
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
  _conn->numLeased.fetch_add(1);
};
  
ConnectionPool::Ref& ConnectionPool::Ref::operator=(Ref& other) {
  if (_conn) {
    _conn->numLeased.fetch_sub(1);
  }
  _conn = other._conn;
  _conn->numLeased.fetch_add(1);
  return *this;
}
 
ConnectionPool::Ref::~Ref() {
  if (_conn) {
    _conn->numLeased.fetch_sub(1);
  }
}

std::shared_ptr<fuerte::Connection> const& ConnectionPool::Ref::connection() const {
  return _conn->fuerte;
}

  
}}
