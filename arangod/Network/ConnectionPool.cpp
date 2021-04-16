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

DECLARE_GAUGE(arangodb_connection_pool_connections_current,
              uint64_t, "Current number of connections in pool");
DECLARE_COUNTER(arangodb_connection_pool_leases_successful_total,
                "Total number of successful connection leases");
DECLARE_COUNTER(arangodb_connection_pool_leases_failed_total,
                "Total number of failed connection leases");
DECLARE_COUNTER(arangodb_connection_pool_connections_created_total,
                "Total number of connections created");

struct LeaseTimeScale {
  static log_scale_t<float> scale() { return {2.f, 0.f, 1000.f, 10}; }
};
DECLARE_HISTOGRAM(
  arangodb_connection_pool_lease_time_hist, LeaseTimeScale,
  "Time to lease a connection from pool [ms]");

namespace arangodb {
namespace network {

using namespace arangodb::fuerte::v1;

ConnectionPool::ConnectionPool(ConnectionPool::Config const& config)
  : _config(config), 
    _loop(config.numIOThreads, config.name),
    _totalConnectionsInPool(
      _config.metricsFeature.add(
        arangodb_connection_pool_connections_current{}.withLabel("pool", _config.name))),
    _successSelect(
      _config.metricsFeature.add(
        arangodb_connection_pool_leases_successful_total{}.withLabel("pool", config.name))),
    _noSuccessSelect(
      _config.metricsFeature.add(
        arangodb_connection_pool_leases_failed_total{}.withLabel("pool", config.name))),
    _connectionsCreated(
      _config.metricsFeature.add(
        arangodb_connection_pool_connections_created_total{}.withLabel("pool", config.name))),
    _leaseHistMSec(
      _config.metricsFeature.add(
        arangodb_connection_pool_lease_time_hist{}.withLabel("pool", config.name))) {
  TRI_ASSERT(config.numIOThreads > 0);
}

ConnectionPool::~ConnectionPool() { shutdownConnections(); }

/// @brief request a connection for a specific endpoint
/// note: it is the callers responsibility to ensure the endpoint
/// is always the same, we do not do any post-processing
network::ConnectionPtr ConnectionPool::leaseConnection(std::string const& endpoint, bool& isFromPool) {
  READ_LOCKER(guard, _lock);
  auto it = _connections.find(endpoint);
  if (it == _connections.end()) {
    guard.unlock();

    auto tmp = std::make_unique<Bucket>(); //get memory outside lock
    WRITE_LOCKER(wguard, _lock);
    auto [it2, emplaced] = _connections.try_emplace(endpoint, std::move(tmp));
    return selectConnection(endpoint,*it2->second, isFromPool);
  }
  return selectConnection(endpoint, *it->second, isFromPool);
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
    for (std::shared_ptr<Context>& c : buck.list) {
      c->fuerte->cancel();
    }
  }
}

/// remove unused and broken connections
void ConnectionPool::pruneConnections() {
  const std::chrono::milliseconds ttl(_config.idleConnectionMilli);

  READ_LOCKER(guard, _lock);
  for (auto& pair : _connections) {

    Bucket& buck = *(pair.second);
    std::lock_guard<std::mutex> lock(buck.mutex);
    
    // get under lock
    auto now = std::chrono::steady_clock::now();

    // this loop removes broken connections, and closes the ones we don't
    // need anymore
    size_t aliveCount = 0;

    // make a single pass over the connections in this bucket
    auto it = buck.list.begin();
    while (it != buck.list.end()) {
      bool remove = false;

      if ((*it)->fuerte->state() == fuerte::Connection::State::Closed) {
        // lets not keep around disconnected fuerte connection objects
        remove = true;
      } else if ((*it)->leases.load() == 0 && (*it)->fuerte->requestsLeft() == 0) {
        if ((now - (*it)->lastLeased) > ttl ||
            aliveCount >= _config.maxOpenConnections) {
          // connection hasn't been used for a while, or there are too many connections
          remove = true;
        } // else keep the connection
      }

      if (remove) {
        it = buck.list.erase(it);
        _totalConnectionsInPool -= 1;
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
    for (std::shared_ptr<Context>& c : buck.list) {
      c->fuerte->cancel();
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

ConnectionPool::Context::Context(std::shared_ptr<fuerte::Connection> c,
                                 std::chrono::steady_clock::time_point t, std::size_t l)
    : fuerte(std::move(c)), lastLeased(t), leases(l) {}

std::shared_ptr<fuerte::Connection> ConnectionPool::createConnection(fuerte::ConnectionBuilder& builder) {
  builder.useIdleTimeout(false);
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
                                               ConnectionPool::Bucket& bucket,
                                               bool& isFromPool) {
  using namespace std::chrono;
  const milliseconds ttl(_config.idleConnectionMilli);
  
  std::lock_guard<std::mutex> guard(bucket.mutex);

  auto start = steady_clock::now();
  
  isFromPool = true;  // Will revert for new collections
  for (std::shared_ptr<Context>& c : bucket.list) {
    if (c->fuerte->state() == fuerte::Connection::State::Closed ||
        (start - c->lastLeased) > ttl) {
      continue;
    }

    TRI_ASSERT(_config.protocol != fuerte::ProtocolType::Undefined);

    std::size_t limit = 0;
    switch (_config.protocol) {
      case fuerte::ProtocolType::Vst:
      case fuerte::ProtocolType::Http2:
        limit = 4;
        break;
      default:
        break;  // keep default of 0
    }

    // first check against number of active users
    std::size_t num = c->leases.load(std::memory_order_relaxed);
    while (num <= limit) {
      bool const leased = c->leases.compare_exchange_strong(num, num + 1, std::memory_order_relaxed);
      if (leased) {
        // next check against the number of requests in flight
        if (c->fuerte->requestsLeft() <= limit &&
            c->fuerte->state() != fuerte::Connection::State::Closed) {
          c->lastLeased = std::chrono::steady_clock::now();
          ++_successSelect;
          _leaseHistMSec.count(
              duration<float, std::micro>(c->lastLeased - start).count());
          return {c};
        } else {  // too many requests,
          c->leases.fetch_sub(1, std::memory_order_relaxed);
          ++_noSuccessSelect;
          break;
        }
      }
    }
  }
  
  ++_connectionsCreated;
  isFromPool = false;
  
  // no free connection found, so we add one
  LOG_TOPIC("2d6ab", DEBUG, Logger::COMMUNICATION) << "creating connection to "
    << endpoint << " bucket size  " << bucket.list.size();

  fuerte::ConnectionBuilder builder;
  builder.endpoint(endpoint); // picks the socket type

  auto now = steady_clock::now();
  auto c = std::make_shared<Context>(createConnection(builder),
                                     now, 1 /* leases*/);
  bucket.list.push_back(c);
  _totalConnectionsInPool += 1;
  _leaseHistMSec.count(duration<float, std::micro>(now - start).count());
  return {c};
}

ConnectionPool::Config const& ConnectionPool::config() const { return _config; }

ConnectionPtr::ConnectionPtr(std::shared_ptr<ConnectionPool::Context>& ctx)
    : _context{ctx} {}

ConnectionPtr::ConnectionPtr(ConnectionPtr&& other)
    : _context(std::move(other._context)) {}

ConnectionPtr::~ConnectionPtr() {
  if (_context) {
    _context->leases.fetch_sub(1, std::memory_order_relaxed);
  }
}

fuerte::Connection& ConnectionPtr::operator*() const {
  return *(_context->fuerte);
}

fuerte::Connection* ConnectionPtr::operator->() const {
  return _context->fuerte.get();
}

fuerte::Connection* ConnectionPtr::get() const {
  return _context->fuerte.get();
}

}  // namespace network
}  // namespace arangodb
