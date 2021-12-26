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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_SIMPLE_HTTP_CLIENT_CONNECTION_CACHE_H
#define ARANGODB_SIMPLE_HTTP_CLIENT_CONNECTION_CACHE_H 1

#include "Basics/Mutex.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace arangodb {
namespace application_features {
class ApplicationServer;
}

namespace httpclient {
class ConnectionCache;
class GeneralClientConnection;

struct ConnectionLease {
  ConnectionLease();
  ConnectionLease(ConnectionCache* cache, std::unique_ptr<GeneralClientConnection> connection);

  ~ConnectionLease();

  ConnectionLease(ConnectionLease const&) = delete;
  ConnectionLease& operator=(ConnectionLease const&) = delete;

  ConnectionLease(ConnectionLease&& other) noexcept;
  ConnectionLease& operator=(ConnectionLease&& other) noexcept;

  /// @brief prevent the connection from being inserted back into the connection cache
  void preventRecycling() noexcept;

  ConnectionCache* _cache;
  std::unique_ptr<GeneralClientConnection> _connection;
  bool _preventRecycling;
};

class ConnectionCache {
  ConnectionCache(ConnectionCache const&) = delete;
  ConnectionCache& operator=(ConnectionCache const&) = delete;

 public:
  struct Options {
    explicit Options(size_t maxConnectionsPerEndpoint)
        : maxConnectionsPerEndpoint(maxConnectionsPerEndpoint) {}

    size_t maxConnectionsPerEndpoint;
  };

  explicit ConnectionCache(arangodb::application_features::ApplicationServer& server,
                           Options const& options);
  ~ConnectionCache();

  ConnectionLease acquire(std::string endpoint, double connectTimeout, double requestTimeout,
                          size_t connectRetries, uint64_t sslProtocol);

  /// @brief the force flag also moves unconnected connections back into the
  /// cache. this is currently used only for testing
  void release(std::unique_ptr<GeneralClientConnection> connection, bool force = false);

#ifdef ARANGODB_USE_GOOGLE_TESTS
  std::unordered_map<std::string, std::vector<std::unique_ptr<GeneralClientConnection>>> const& connections() const {
    return _connections;
  }
#endif

 private:
  arangodb::application_features::ApplicationServer& _server;

  Options const _options;

  mutable arangodb::Mutex _lock;

  std::unordered_map<std::string, std::vector<std::unique_ptr<GeneralClientConnection>>> _connections;

  uint64_t _connectionsCreated;
  uint64_t _connectionsRecycled;
};

}  // namespace httpclient
}  // namespace arangodb

#endif
