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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <fuerte/connection.h>
#include <fuerte/loop.h>
#include <fuerte/types.h>

#include <chrono>
#include <memory>

#include "Basics/Result.h"
#include "Basics/ResultT.h"
#include "Utils/ConnectionManager.h"
#include "Utils/RequestCoding.h"

#include <atomic>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace arangodb {
class ClientFeature;

namespace application_features {
class ApplicationServer;
}

namespace fuzzer {
class RequestFuzzer;
}

namespace httpclient {
class GeneralClientConnection;
class SimpleHttpClient;
}  // namespace httpclient

namespace fuerte {
inline namespace v1 {
class Connection;
}
}  // namespace fuerte

////////////////////////////////////////////////////////////////////////////////
/// @brief class for http requests
////////////////////////////////////////////////////////////////////////////////

class ClientConnection {
  ClientConnection(ClientConnection const&) = delete;
  ClientConnection& operator=(ClientConnection const&) = delete;

 public:
  explicit ClientConnection(application_features::ApplicationServer&,
                            ClientFeature&);
  ~ClientConnection();

  void setInterrupted(bool interrupted);

  bool isConnected() const;

  ResultT<std::string> prepareConnection();
  ResultT<std::string> connect();
  ResultT<std::string> reconnect();

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  void reconnectWithNewPassword(std::string const& password);
#endif
  double timeout() const;

  void timeout(double value);

  std::string protocol() const;

  /// @brief the database of the active connection, which it keeps while
  /// inactive
  std::string databaseName() const;
  void setDatabaseName(std::string const& value);
  void setForceJson(bool value) { _forceJson = value; };
  /// @brief endpoint and credentials of the active connection, as configured
  /// when it was created; those of the client before the first connect
  std::string endpoint() const;
  std::string username() const;
  std::string password() const;
  int lastHttpReturnCode() const { return _lastHttpReturnCode; }
  std::string lastErrorMessage() const { return _lastErrorMessage; }
  std::string const& version() const { return _version; }
  std::string const& mode() const { return _mode; }
  std::string const& role() const { return _role; }
  std::string endpointSpecification() const;

  std::string getLocalEndpoint() {
    if (auto const& connection = _manager.activeConnection(); connection) {
      return connection->localEndpoint();
    } else {
      return "not connected";
    }
  }
  application_features::ApplicationServer& server();

  struct RequestOutcome {
    // uncompressed; null if no response arrived
    std::unique_ptr<fuerte::Response> response;
    // why no response arrived
    Result error;
  };

  // Sends a request to the database of the active connection, once more if
  // the server closed the connection meanwhile. Fails if the request cannot
  // be sent, or its response cannot be uncompressed
  ResultT<RequestOutcome> sendRequest(
      fuerte::RestVerb verb, std::string_view location,
      std::unordered_map<std::string, std::string> const& headerFields,
      request_coding::RequestBody const& body,
      std::chrono::milliseconds timeout);

  std::string getHandle();

  using HandleInfo = ConnectionManager::HandleInfo;
  std::vector<HandleInfo> connectionHandleTable() const;

  using HandleSwitch = ConnectionManager::HandleSwitch;
  HandleSwitch connectHandle(std::string const& handle);

  // false if the handle is unknown
  bool disconnectHandle(std::string const& handle);
  void flushConnectionCache();

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  // sendFuzzRequest() yields the HTTP status of the response, or one of these
  static constexpr uint32_t kFuzzClosedConnectionCode = 1000;
  static constexpr uint32_t kFuzzNoResponseCode = 1001;
  static constexpr uint32_t kFuzzNotConnected = 1002;

  uint32_t sendFuzzRequest(fuzzer::RequestFuzzer& fuzzer);
#endif

  // forces a new connection to be used
  void forceNewConnection();

 private:
  std::shared_ptr<fuerte::Connection> createConnection(
      bool bypassCache = false);
  std::shared_ptr<fuerte::Connection> acquireConnection(bool bypassCache);

  /// @brief cancels the active connection, which stays active but closed
  /// until it is replaced
  void shutdownConnection();

  void setCustomError(int httpCode, std::string const& msg) {
    _setCustomError = true;
    _lastHttpReturnCode = httpCode;
    _lastErrorMessage = msg;
  }

  // Renews the token of the active configuration if due, and switches to a
  // connection with the new token
  void renewTokenIfDue();

  // Points the client settings at the endpoint, credentials and database of
  // settings, so that whatever reads them, such as the importers, the prompt
  // and reconnect() defaults, follows the active handle
  void adoptClientSettings(ConnectionSettings const& settings);

  std::chrono::milliseconds requestTimeout() const;

 private:
  application_features::ApplicationServer& _server;
  ClientFeature& _client;

  std::chrono::duration<double> _requestTimeout;

  mutable std::recursive_mutex _lock;
  unsigned _lastHttpReturnCode;
  std::string _lastErrorMessage;
  std::string _version;
  std::string _mode;
  std::string _role;

  fuerte::EventLoopService _loop;
  bool _foundConnectionClose;
  bool _forceJson;
  std::atomic<bool> _setCustomError;

  // whenever we reconnect to another endpoint, the old connection is kept
  // for reuse, so as not to use too many separate connections and ephemeral
  // ports for patterns such as "connect-to-leader" -> "connect-to-follower" ->
  // "connect-to-leader". declared after _loop, which its connections run on
  ConnectionManager _manager;
};
}  // namespace arangodb
