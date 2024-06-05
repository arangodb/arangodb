////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include "Auth/TokenCache.h"
#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "Containers/SmallVector.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Logger/LogMacros.h"
#include "Metrics/CounterBuilder.h"
#include "Metrics/GaugeBuilder.h"
#include "Metrics/HistogramBuilder.h"
#include "Metrics/LogScale.h"
#include "Metrics/MetricsFeature.h"
#include "Network/NetworkFeature.h"

#include <fuerte/connection.h>
#include <fuerte/loop.h>
#include <fuerte/types.h>

#include <memory>
#include <vector>

namespace arangodb::network {

using namespace arangodb::fuerte::v1;

struct ConnectionPool::Context {
  Context(std::shared_ptr<fuerte::Connection>,
          std::chrono::steady_clock::time_point, std::size_t);

  std::shared_ptr<fuerte::Connection> fuerte;
  std::chrono::steady_clock::time_point lastLeased;  /// last time leased
  std::atomic<std::size_t> leases;  // number of active users, including those
                                    // who may not have sent a request yet
};

struct ConnectionPool::Bucket {
  mutable std::mutex mutex;
  containers::SmallVector<std::shared_ptr<Context>, 4> list;
};

struct ConnectionPool::Impl {
  Impl(Impl const& other) = delete;
  Impl& operator=(Impl const& other) = delete;

  explicit Impl(ConnectionPool::Config const& config, ConnectionPool& pool)
      : _config(config),
        _pool(pool),
        _loop(config.numIOThreads, config.name),
        _stopped(false),
        _totalConnectionsInPool(*_config.metrics.totalConnectionsInPool),
        _successSelect(*_config.metrics.successSelect),
        _noSuccessSelect(*_config.metrics.noSuccessSelect),
        _connectionsCreated(*_config.metrics.connectionsCreated),
        _leaseHistMSec(*_config.metrics.leaseHistMSec) {
    TRI_ASSERT(config.numIOThreads > 0);
  }

  /// @brief request a connection for a specific endpoint
  /// note: it is the callers responsibility to ensure the endpoint
  /// is always the same, we do not do any post-processing
  network::ConnectionPtr leaseConnection(std::string const& endpoint,
                                         bool& isFromPool) {
    READ_LOCKER(guard, _lock);
    if (_stopped) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_SHUTTING_DOWN,
                                     "connection pool already stopped");
    }

    auto it = _connections.find(endpoint);
    if (it == _connections.end()) {
      guard.unlock();

      auto tmp = std::make_unique<Bucket>();  // get memory outside lock
      WRITE_LOCKER(wguard, _lock);
      auto [it2, emplaced] = _connections.try_emplace(endpoint, std::move(tmp));
      return selectConnection(endpoint, *it2->second, isFromPool);
    }
    return selectConnection(endpoint, *it->second, isFromPool);
  }

  /// @brief stops the connection pool
  void stop() {
    {
      WRITE_LOCKER(guard, _lock);
      _stopped = true;
    }
    drainConnections();
    _loop.stop();
  }

  /// @brief drain all connections
  void drainConnections() {
    WRITE_LOCKER(guard, _lock);
    size_t n = 0;
    for (auto& pair : _connections) {
      Bucket& buck = *(pair.second);
      std::lock_guard<std::mutex> lock(buck.mutex);
      n += buck.list.size();
      buck.list.clear();
    }
    // We drop everything.
    TRI_ASSERT(_totalConnectionsInPool.load() == n);
    _totalConnectionsInPool -= n;
    _connections.clear();
  }

  /// @brief shutdown all connections
  void shutdownConnections() {
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
  void pruneConnections() {
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
        } else if ((*it)->leases.load() == 0 &&
                   (*it)->fuerte->requestsLeft() == 0) {
          if ((now - (*it)->lastLeased) > ttl ||
              aliveCount >= _config.maxOpenConnections) {
            // connection hasn't been used for a while, or there are too many
            // connections
            remove = true;
          }  // else keep the connection
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
                << "pruning extra connections to '" << pair.first << "' ("
                << buck.list.size() << ")";
          }
        }
      }
    }
  }

  /// @brief cancel connections to this endpoint
  size_t cancelConnections(std::string const& endpoint) {
    WRITE_LOCKER(guard, _lock);
    auto const& it = _connections.find(endpoint);
    if (it != _connections.end()) {
      size_t n;
      {
        Bucket& buck = *(it->second);
        std::lock_guard<std::mutex> lock(buck.mutex);
        n = buck.list.size();
        for (std::shared_ptr<Context>& c : buck.list) {
          c->fuerte->cancel();
        }
      }
      _connections.erase(it);
      // We just erased `n` connections on the bucket.
      // Let's count it.
      TRI_ASSERT(_totalConnectionsInPool.load() >= n);
      _totalConnectionsInPool -= n;
      return n;
    }
    return 0;
  }

  /// @brief return the number of open connections
  size_t numOpenConnections() const {
    size_t conns = 0;

    READ_LOCKER(guard, _lock);
    for (auto& pair : _connections) {
      Bucket& buck = *(pair.second);
      std::lock_guard<std::mutex> lock(buck.mutex);
      conns += buck.list.size();
    }
    return conns;
  }

  ConnectionPtr selectConnection(std::string const& endpoint,
                                 ConnectionPool::Bucket& bucket,
                                 bool& isFromPool) {
    using namespace std::chrono;
    milliseconds const ttl(_config.idleConnectionMilli);

    auto start = steady_clock::now();
    isFromPool = true;  // Will revert for new connections

    // exclusively lock the bucket
    std::unique_lock<std::mutex> guard(bucket.mutex);

    for (std::shared_ptr<Context>& c : bucket.list) {
      if (c->fuerte->state() == fuerte::Connection::State::Closed ||
          (start - c->lastLeased) > ttl) {
        continue;
      }

      TRI_ASSERT(_config.protocol != fuerte::ProtocolType::Undefined);

      std::size_t limit = 0;
      switch (_config.protocol) {
        case fuerte::ProtocolType::Http2:
          limit = 4;
          break;
        default:
          break;  // keep default of 0
      }

      // first check against number of active users
      std::size_t num = c->leases.load(std::memory_order_relaxed);
      while (num <= limit) {
        bool const leased = c->leases.compare_exchange_strong(
            num, num + 1, std::memory_order_relaxed);
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
    // no free connection found, so we add one
    LOG_TOPIC("2d6ab", DEBUG, Logger::COMMUNICATION)
        << "creating connection to " << endpoint << " bucket size  "
        << bucket.list.size();

    fuerte::ConnectionBuilder builder;
    builder.endpoint(endpoint);  // picks the socket type

    auto now = steady_clock::now();
    auto c = std::make_shared<Context>(_pool.createConnection(builder), now,
                                       1 /* leases*/);
    bucket.list.push_back(c);

    guard.unlock();
    // continue without the bucket lock

    isFromPool = false;
    _totalConnectionsInPool += 1;
    _leaseHistMSec.count(duration<float, std::micro>(now - start).count());
    return {std::move(c)};
  }

  std::shared_ptr<fuerte::Connection> createConnection(
      fuerte::ConnectionBuilder& builder) {
    builder.useIdleTimeout(false);
    builder.verifyHost(_config.verifyHosts);
    builder.protocolType(_config.protocol);  // always overwrite protocol
    TRI_ASSERT(builder.socketType() != SocketType::Undefined);

    AuthenticationFeature* af = AuthenticationFeature::instance();
    if (af != nullptr && af->isActive()) {
      std::string const& token = af->tokenCache().jwtToken();
      if (!token.empty()) {
        builder.jwtToken(token);
        builder.authenticationType(fuerte::AuthenticationType::Jwt);
      }
    }
    return builder.connect(_loop);
  }

  Config const _config;
  ConnectionPool& _pool;

  mutable basics::ReadWriteLock _lock;
  /// @brief map from endpoint to a bucket with connections to the endpoint.
  /// protected by _lock.
  std::unordered_map<std::string, std::unique_ptr<Bucket>> _connections;

  /// @brief contains fuerte asio::io_context
  fuerte::EventLoopService _loop;

  /// @brief whether or not the connection pool was already stopped. if set
  /// to true, calling leaseConnection will throw an exception. protected
  /// by _lock.
  bool _stopped;

  metrics::Gauge<uint64_t>& _totalConnectionsInPool;
  metrics::Counter& _successSelect;
  metrics::Counter& _noSuccessSelect;
  metrics::Counter& _connectionsCreated;

  metrics::Histogram<metrics::LogScale<float>>& _leaseHistMSec;
};

ConnectionPool::ConnectionPool(ConnectionPool::Config const& config)
    : _impl(std::make_unique<Impl>(config, *this)) {}

ConnectionPool::~ConnectionPool() {
  shutdownConnections();
  stop();
}

/// @brief request a connection for a specific endpoint
/// note: it is the callers responsibility to ensure the endpoint
/// is always the same, we do not do any post-processing
network::ConnectionPtr ConnectionPool::leaseConnection(
    std::string const& endpoint, bool& isFromPool) {
  return _impl->leaseConnection(endpoint, isFromPool);
}

/// @brief stops the connection pool (also calls drainConnections)
void ConnectionPool::stop() { _impl->stop(); }

/// @brief drain all connections
void ConnectionPool::drainConnections() { _impl->drainConnections(); }

/// @brief shutdown all connections
void ConnectionPool::shutdownConnections() { _impl->shutdownConnections(); }

/// remove unused and broken connections
void ConnectionPool::pruneConnections() { _impl->pruneConnections(); }

/// @brief cancel connections to this endpoint
size_t ConnectionPool::cancelConnections(std::string const& endpoint) {
  return _impl->cancelConnections(endpoint);
}

/// @brief return the number of open connections
size_t ConnectionPool::numOpenConnections() const {
  return _impl->numOpenConnections();
}

std::shared_ptr<fuerte::Connection> ConnectionPool::createConnection(
    fuerte::ConnectionBuilder& builder) {
  return _impl->createConnection(builder);
}

ConnectionPool::Config const& ConnectionPool::config() const {
  return _impl->_config;
}

ConnectionPool::Context::Context(std::shared_ptr<fuerte::Connection> c,
                                 std::chrono::steady_clock::time_point t,
                                 std::size_t l)
    : fuerte(std::move(c)), lastLeased(t), leases(l) {}

ConnectionPtr::ConnectionPtr(std::shared_ptr<ConnectionPool::Context> ctx)
    : _context{std::move(ctx)} {}

ConnectionPtr::ConnectionPtr(ConnectionPtr&& other) noexcept
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

DECLARE_GAUGE(arangodb_connection_pool_connections_current, uint64_t,
              "Current number of connections in pool");
DECLARE_COUNTER(arangodb_connection_pool_leases_successful_total,
                "Total number of successful connection leases");
DECLARE_COUNTER(arangodb_connection_pool_leases_failed_total,
                "Total number of failed connection leases");
DECLARE_COUNTER(arangodb_connection_pool_connections_created_total,
                "Total number of connections created");

struct LeaseTimeScale {
  static arangodb::metrics::LogScale<float> scale() {
    return {2.f, 0.f, 1000.f, 10};
  }
};
DECLARE_HISTOGRAM(arangodb_connection_pool_lease_time_hist, LeaseTimeScale,
                  "Time to lease a connection from pool [ms]");

namespace {
template<typename Gen>
ConnectionPool::Metrics createMetrics(Gen&& g, std::string_view name) {
  ConnectionPool::Metrics m;
  m.totalConnectionsInPool =
      g(arangodb_connection_pool_connections_current{}.withLabel("pool", name));
  m.successSelect =
      g(arangodb_connection_pool_leases_successful_total{}.withLabel("pool",
                                                                     name));
  m.noSuccessSelect =
      g(arangodb_connection_pool_leases_failed_total{}.withLabel("pool", name));
  m.connectionsCreated =
      g(arangodb_connection_pool_connections_created_total{}.withLabel("pool",
                                                                       name));
  m.leaseHistMSec =
      g(arangodb_connection_pool_lease_time_hist{}.withLabel("pool", name));
  return m;
}

}  // namespace

ConnectionPool::Metrics ConnectionPool::Metrics::fromMetricsFeature(
    metrics::MetricsFeature& metricsFeature, std::string_view name) {
  return createMetrics(
      [&](auto&& builder) { return &metricsFeature.add(std::move(builder)); },
      name);
}

ConnectionPool::Metrics ConnectionPool::Metrics::createStub(
    std::string_view name) {
  return createMetrics(
      [&]<typename Builder>(Builder&& builder) {
        static std::vector<std::shared_ptr<typename Builder::MetricT>> metrics;
        auto ptr = std::dynamic_pointer_cast<typename Builder::MetricT>(
            Builder{}.build());
        return metrics.emplace_back(ptr).get();
      },
      name);
}

}  // namespace arangodb::network
