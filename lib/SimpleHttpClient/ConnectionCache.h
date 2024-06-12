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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "SimpleHttpClient/GeneralClientConnection.h"

namespace arangodb {
namespace application_features {
class ApplicationServer;
class CommunicationFeaturePhase;
}  // namespace application_features

namespace httpclient {
class ConnectionCache;

struct ConnectionLease {
  ConnectionLease();
  ConnectionLease(ConnectionCache* cache,
                  std::unique_ptr<GeneralClientConnection> connection);

  ~ConnectionLease();

  ConnectionLease(ConnectionLease const&) = delete;
  ConnectionLease& operator=(ConnectionLease const&) = delete;

  ConnectionLease(ConnectionLease&& other) noexcept;
  ConnectionLease& operator=(ConnectionLease&& other) noexcept;

  /// @brief prevent the connection from being inserted back into the connection
  /// cache
  void preventRecycling() noexcept;

  ConnectionCache* _cache;
  std::unique_ptr<GeneralClientConnection> _connection;
  std::atomic<bool> _preventRecycling;
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

  ConnectionCache(application_features::CommunicationFeaturePhase& comm,
                  Options const& options);

  ConnectionLease acquire(std::string endpoint, double connectTimeout,
                          double requestTimeout, size_t connectRetries,
                          uint64_t sslProtocol);

  /// @brief the force flag also moves unconnected connections back into the
  /// cache. this is currently used only for testing
  void release(std::unique_ptr<GeneralClientConnection> connection,
               bool force = false);

#ifdef ARANGODB_USE_GOOGLE_TESTS
  std::unordered_map<
      std::string, std::vector<std::unique_ptr<GeneralClientConnection>>> const&
  connections() const {
    return _connections;
  }
#endif

 private:
  application_features::CommunicationFeaturePhase& _comm;

  Options const _options;

  mutable std::mutex _lock;

  std::unordered_map<std::string,
                     std::vector<std::unique_ptr<GeneralClientConnection>>>
      _connections;

  uint64_t _connectionsCreated;
  uint64_t _connectionsRecycled;
};

}  // namespace httpclient
}  // namespace arangodb
