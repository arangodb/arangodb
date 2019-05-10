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

#ifndef ARANGOD_NETWORK_CONNECTION_POOL_H
#define ARANGOD_NETWORK_CONNECTION_POOL_H 1

#include "Basics/ReadWriteSpinLock.h"
#include "Network/types.h"
#include "VocBase/voc-types.h"

#include <fuerte/loop.h>
#include <fuerte/types.h>
#include <atomic>
#include <chrono>

namespace arangodb {
namespace fuerte {
inline namespace v1 {
class Connection;
class ConnectionBuilder;
}  // namespace v1
}  // namespace fuerte

namespace network {

/// @brief simple connection pool managing fuerte connections
#ifdef ARANGODB_USE_CATCH_TESTS
class ConnectionPool {
#else
class ConnectionPool final {
#endif
 protected:
  struct Connection;

 public:
  struct Config {
    uint64_t numIOThreads = 1;            /// number of IO threads
    uint64_t minOpenConnections = 1;      /// minimum number of open connections
    uint64_t maxOpenConnections = 25;     /// max number of connections
    uint64_t connectionTtlMilli = 60000;  /// unused connection lifetime
    uint64_t requestTimeoutMilli = 120000; /// request timeout
    bool verifyHosts = false;
    fuerte::ProtocolType protocol = fuerte::ProtocolType::Http;
  };

  /// @brief simple connection reference counter
  struct Ref {
    Ref(ConnectionPool::Connection* c);
    Ref(Ref&& r);
    Ref& operator=(Ref&&);
    Ref(Ref const& other);
    Ref& operator=(Ref&);
    ~Ref();

    std::shared_ptr<fuerte::Connection> connection() const;

   private:
    ConnectionPool::Connection* _conn;  // back reference to list
  };

 public:
  ConnectionPool(ConnectionPool::Config const& config);
  virtual ~ConnectionPool();

  /// @brief request a connection for a specific endpoint
  /// note: it is the callers responsibility to ensure the endpoint
  /// is always the same, we do not do any post-processing
  Ref leaseConnection(EndpointSpec const&);

  /// @brief event loop service to create a connection seperately
  /// user is responsible for correctly shutting it down
  fuerte::EventLoopService& eventLoopService() { return _loop; }

  /// @brief shutdown all connections
  void shutdown();

  /// @brief automatically prune connections
  void pruneConnections();
  
  /// @brief cancel connections to this endpoint
  void cancelConnections(EndpointSpec const&);

  /// @brief return the number of open connections
  size_t numOpenConnections() const;

 protected:
  /// @brief connection container
  struct Connection {
    Connection(std::shared_ptr<fuerte::Connection> f)
        : fuerte(std::move(f)),
          numLeased(0),
          lastUsed(std::chrono::steady_clock::now()) {}
    Connection(Connection const& c)
        : fuerte(c.fuerte), numLeased(c.numLeased.load()), lastUsed(c.lastUsed) {}
    Connection& operator=(Connection const& other) {
      this->fuerte = other.fuerte;
      this->numLeased.store(other.numLeased.load());
      this->lastUsed = other.lastUsed;
      return *this;
    }

    std::shared_ptr<fuerte::Connection> fuerte;
    std::atomic<size_t> numLeased;
    std::chrono::steady_clock::time_point lastUsed;
  };

  /// @brief
  struct ConnectionList {
    std::mutex mutex;
    // TODO statistics ?
    //    uint64_t bytesSend;
    //    uint64_t bytesReceived;
    //    uint64_t numRequests;
    std::vector<std::unique_ptr<Connection>> connections;
  };

  TEST_VIRTUAL std::shared_ptr<fuerte::Connection> createConnection(fuerte::ConnectionBuilder&);
  Ref selectConnection(ConnectionList&, fuerte::ConnectionBuilder& builder);
  
  void removeBrokenConnections(ConnectionList&);

 private:
  const Config _config;

  mutable basics::ReadWriteSpinLock _lock;
  std::unordered_map<std::string, std::unique_ptr<ConnectionList>> _connections;

  /// @brief contains fuerte asio::io_context
  fuerte::EventLoopService _loop;
  /// @brief
  //  asio_ns::steady_timer _pruneTimer;
};

}  // namespace network
}  // namespace arangodb

#endif
