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
#include "Cluster/ClusterInfo.h"
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
      _loop(config.numIOThreads, config.name),
      _totalConnectionsInPool(nullptr),
      _successSelect(nullptr),
      _noSuccessSelect(nullptr),
      _connectionsCreated(nullptr),
      _leaseHistMSec(nullptr) {
  TRI_ASSERT(config.numIOThreads > 0);
  TRI_ASSERT(_config.minOpenConnections <= _config.maxOpenConnections);
  if (_config.clusterInfo != nullptr) {
    _totalConnectionsInPool =
      &_config.clusterInfo->server().getFeature<arangodb::MetricsFeature>().gauge(
        std::string("arangodb_connection_pool_nr_conns_") + _config.name, uint64_t(0), "Current number of connections in pool");
    _successSelect =
      &_config.clusterInfo->server().getFeature<arangodb::MetricsFeature>().counter(
        std::string("arangodb_connection_pool_success_select_") + _config.name, 0, "Total number of successful connection leases");
    _noSuccessSelect =
      &_config.clusterInfo->server().getFeature<arangodb::MetricsFeature>().counter(
        std::string("arangodb_connection_pool_no_success_select_") + _config.name, 0, "Total number of failed connection leases");
    _connectionsCreated =
      &_config.clusterInfo->server().getFeature<arangodb::MetricsFeature>().counter(
        std::string("arangodb_connection_pool_conns_created_") + _config.name, 0, "Number of connections created");
    _leaseHistMSec =
      &_config.clusterInfo->server().getFeature<arangodb::MetricsFeature>().histogram(
        std::string("arangodb_connection_pool_lease_time_hist_")+ _config.name, log_scale_t(2.f, 0.f, 1000.f, 10),
        std::string("Time to lease a connection from pool ") + _config.name + " [us]");
  }
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
    for (std::shared_ptr<Context>& c : buck.list) {
      c->fuerte->cancel();
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

      if ((*it)->fuerte->state() == fuerte::Connection::State::Failed) {
        // lets not keep around disconnected fuerte connection objects
        remove = true;
      } else {
        // connection has not yet failed
        if ((*it)->leases.load() > 0 || (*it)->fuerte->requestsLeft() > 0) {  // continuously update lastUsed
          (*it)->lastLeased = now;
          // connection will be kept
        } else if (aliveCount >= _config.minOpenConnections &&
                   (now - (*it)->lastLeased) > ttl) {
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
        if (_totalConnectionsInPool) {
          _totalConnectionsInPool -= 1;
        }
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

  using namespace std::chrono;
  std::lock_guard<std::mutex> guard(bucket.mutex);

  auto start = steady_clock::now();

  for (std::shared_ptr<Context>& c : bucket.list) {
    const fuerte::Connection::State state = c->fuerte->state();
    if (state == fuerte::Connection::State::Failed) {
      continue;
    }

    if (!c->fuerte->lease()) {
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
    std::size_t num = c->leases.load();
    while (num <= limit) {
      bool const leased = c->leases.compare_exchange_strong(num, num + 1);
      if (leased) {
        // next check against the number of requests in flight
        if (c->fuerte->requestsLeft() <= limit) {
          c->lastLeased = std::chrono::steady_clock::now();
          if (_successSelect) {
            _successSelect++;
          }
          if (_leaseHistMSec) {
            _leaseHistMSec->count(
              duration<float, std::micro>(c->lastLeased - start).count());
          }
          return {c};
        } else {
          --(c->leases);
          c->fuerte->unlease();
          if (_noSuccessSelect) {
            _noSuccessSelect++;
          }
          break;
        }
      }
    }
  }

  if (_connectionsCreated) {
    _connectionsCreated++;
  }

  // no free connection found, so we add one
  LOG_TOPIC("2d6ab", DEBUG, Logger::COMMUNICATION) << "creating connection to "
    << endpoint << " bucket size  " << bucket.list.size();

  fuerte::ConnectionBuilder builder;
  builder.endpoint(endpoint); // picks the socket type

  std::shared_ptr<fuerte::Connection> fuerte = createConnection(builder);
  auto now = steady_clock::now();
  auto c = std::make_shared<Context>(fuerte, now, 1 /* leases*/);
  bucket.list.push_back(c);
  if (_totalConnectionsInPool) {
    _totalConnectionsInPool += 1;
  }
  
  if (_leaseHistMSec) {
    _leaseHistMSec->count(duration<float, std::micro>(now - start).count());
  }
  return {c};
}

ConnectionPool::Config const& ConnectionPool::config() const { return _config; }

ConnectionPtr::ConnectionPtr(std::shared_ptr<ConnectionPool::Context>& ctx)
    : _context{ctx} {}

ConnectionPtr::ConnectionPtr(ConnectionPtr&& other)
    : _context(std::move(other._context)) {}

ConnectionPtr::~ConnectionPtr() {
  if (_context) {
    --(_context->leases);
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
