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

#ifndef ARANGOD_NETWORK_CONNECTION_POOL_H
#define ARANGOD_NETWORK_CONNECTION_POOL_H 1

#include "Basics/ReadWriteSpinLock.h"
#include "VocBase/voc-types.h"

#include <atomic>
#include <chrono>
#include <fuerte/loop.h>
#include <fuerte/types.h>

namespace arangodb {
namespace fuerte { inline namespace v1 {
  class Connection;
  class ConnectionBuilder;
}}
class NetworkFeature;
  
namespace network {
  
/// @brief unified endpoint 
typedef std::string EndpointSpec;

class ConnectionPool {
  struct Connection;

public:
  
  struct Ref {
    Ref(ConnectionPool::Connection* c) : _conn(c) {}
    Ref(Ref&& r);
    Ref& operator=(Ref&&);
    Ref(Ref const& other);
    Ref& operator=(Ref&);
    ~Ref();
    
    std::shared_ptr<fuerte::Connection> const& connection() const;

   private:
    ConnectionPool::Connection* _conn; // back reference to list
  };
  
public:
  
  ConnectionPool(NetworkFeature*);
  
  /// @brief request a connection for a specific endpoint
  /// note: it is the callers responsibility to ensure the endpoint
  /// is always the same, we do not do any post-processing
  Ref leaseConnection(EndpointSpec);
  
  /// @brief shutdown all connections
  void shutdown();
  
private:
  
  struct Connection {
    Connection(std::shared_ptr<fuerte::Connection> f)
      : fuerte(std::move(f)), numLeased(0),
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
    std::vector<Connection> connections;
  };
  
  void pruneConnections();
  
  std::shared_ptr<fuerte::Connection> createConnection(fuerte::ConnectionBuilder&);
  Ref selectConnection(ConnectionList&, fuerte::ConnectionBuilder& builder);
  
private:
  NetworkFeature* _network;

  basics::ReadWriteSpinLock _lock;
  std::unordered_map<std::string, std::unique_ptr<ConnectionList>> _connections;
  
  fuerte::EventLoopService _loop;
};
  
}}

#endif
