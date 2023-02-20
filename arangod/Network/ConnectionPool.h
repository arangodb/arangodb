////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#pragma once

#include "Basics/Common.h"
#include "Basics/ReadWriteLock.h"
#include "Containers/SmallVector.h"
#include "Network/types.h"
#include "Metrics/Fwd.h"
#include "VocBase/voc-types.h"

#include <fuerte/loop.h>
#include <fuerte/types.h>
#include <atomic>
#include <chrono>
#include <unordered_map>

namespace arangodb {
namespace fuerte {
inline namespace v1 {
class Connection;
class ConnectionBuilder;
}  // namespace v1
}  // namespace fuerte
class ClusterInfo;

namespace network {

// using ConnectionPtr = std::shared_ptr<fuerte::Connection>;
class ConnectionPtr;

/// @brief simple connection pool managing fuerte connections
#ifdef ARANGODB_USE_GOOGLE_TESTS
class ConnectionPool {
#else
class ConnectionPool final {
#endif
 protected:
  struct Connection;
  friend class ConnectionPtr;

 public:
  struct Config {
    ClusterInfo* clusterInfo;
    metrics::MetricsFeature& metricsFeature;
    uint64_t maxOpenConnections = 1024;     /// max number of connections
    uint64_t idleConnectionMilli = 120000;  /// unused connection lifetime
    unsigned int numIOThreads = 1;          /// number of IO threads
    bool verifyHosts = false;
    fuerte::ProtocolType protocol = fuerte::ProtocolType::Http;
    char const* name = "";
    Config(metrics::MetricsFeature& metricsFeature)
        : metricsFeature(metricsFeature) {}
  };

 public:
  explicit ConnectionPool(ConnectionPool::Config const& config);
  TEST_VIRTUAL ~ConnectionPool();

  /// @brief request a connection for a specific endpoint
  /// note: it is the callers responsibility to ensure the endpoint
  /// is always the same, we do not do any post-processing
  ConnectionPtr leaseConnection(std::string const& endpoint, bool& isFromPool);

  /// @brief event loop service to create a connection separately
  /// user is responsible for correctly shutting it down
  fuerte::EventLoopService& eventLoopService() { return _loop; }

  /// @brief shutdown all connections
  void drainConnections();

  /// @brief shutdown all connections
  void shutdownConnections();

  /// @brief automatically prune connections
  void pruneConnections();

  /// @brief cancel connections to this endpoint
  size_t cancelConnections(std::string const& endpoint);

  /// @brief return the number of open connections
  size_t numOpenConnections() const;

  Config const& config() const;

 protected:
  struct Context {
    Context(std::shared_ptr<fuerte::Connection>,
            std::chrono::steady_clock::time_point, std::size_t);

    std::shared_ptr<fuerte::Connection> fuerte;
    std::chrono::steady_clock::time_point lastLeased;  /// last time leased
    std::atomic<std::size_t> leases;  // number of active users, including those
                                      // who may not have sent a request yet
  };

  /// @brief endpoint bucket
  struct Bucket {
    mutable std::mutex mutex;
    // TODO statistics ?
    //    uint64_t bytesSend;
    //    uint64_t bytesReceived;
    //    uint64_t numRequests;
    containers::SmallVector<std::shared_ptr<Context>, 4> list;
  };

  TEST_VIRTUAL std::shared_ptr<fuerte::Connection> createConnection(
      fuerte::ConnectionBuilder&);
  ConnectionPtr selectConnection(std::string const& endpoint, Bucket& bucket,
                                 bool& isFromPool);

 private:
  Config const _config;

  mutable basics::ReadWriteLock _lock;
  std::unordered_map<std::string, std::unique_ptr<Bucket>> _connections;

  /// @brief contains fuerte asio::io_context
  fuerte::EventLoopService _loop;

  metrics::Gauge<uint64_t>& _totalConnectionsInPool;
  metrics::Counter& _successSelect;
  metrics::Counter& _noSuccessSelect;
  metrics::Counter& _connectionsCreated;

  metrics::Histogram<metrics::LogScale<float>>& _leaseHistMSec;
};

class ConnectionPtr {
 public:
  ConnectionPtr(std::shared_ptr<ConnectionPool::Context> context);
  ConnectionPtr(ConnectionPtr&& ctx) noexcept;
  ConnectionPtr(ConnectionPtr const&) = delete;
  ~ConnectionPtr();

  ConnectionPtr operator=(ConnectionPtr&&) = delete;
  ConnectionPtr operator=(ConnectionPtr const&) = delete;

  fuerte::Connection& operator*() const;
  fuerte::Connection* operator->() const;
  fuerte::Connection* get() const;

 private:
  std::shared_ptr<ConnectionPool::Context> _context;
};

}  // namespace network
}  // namespace arangodb
