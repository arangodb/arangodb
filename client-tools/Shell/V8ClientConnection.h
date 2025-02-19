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
/// @author Dr. Frank Celler
/// @author Achim Brandt
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Shell/arangosh.h"
#include <fuerte/connection.h>
#include <fuerte/loop.h>
#include <fuerte/types.h>
#include <v8.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

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

class V8ClientConnection {
  V8ClientConnection(V8ClientConnection const&) = delete;
  V8ClientConnection& operator=(V8ClientConnection const&) = delete;

 public:
  explicit V8ClientConnection(ArangoshServer&, ClientFeature&);
  ~V8ClientConnection();

  void setInterrupted(bool interrupted);

  bool isConnected() const;

  void connect();
  void reconnect();

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  void reconnectWithNewPassword(std::string const& password);
#endif
  double timeout() const;

  void timeout(double value);

  std::string protocol() const;

  std::string const& databaseName() const { return _databaseName; }
  void setDatabaseName(std::string const& value);
  void setForceJson(bool value) { _forceJson = value; };
  std::string username() const { return _builder.user(); }
  std::string password() const { return _builder.password(); }
  int lastHttpReturnCode() const { return _lastHttpReturnCode; }
  std::string lastErrorMessage() const { return _lastErrorMessage; }
  std::string const& version() const { return _version; }
  std::string const& mode() const { return _mode; }
  std::string const& role() const { return _role; }
  std::string endpointSpecification() const;

  ArangoshServer& server();

  v8::Handle<v8::Value> getData(
      v8::Isolate* isolate, std::string_view location,
      std::unordered_map<std::string, std::string> const& headerFields,
      bool raw);

  v8::Handle<v8::Value> headData(
      v8::Isolate* isolate, std::string_view location,
      std::unordered_map<std::string, std::string> const& headerFields,
      bool raw);

  v8::Handle<v8::Value> deleteData(
      v8::Isolate* isolate, std::string_view location,
      v8::Local<v8::Value> const& body,
      std::unordered_map<std::string, std::string> const& headerFields,
      bool raw);

  v8::Handle<v8::Value> optionsData(
      v8::Isolate* isolate, std::string_view location,
      v8::Local<v8::Value> const& body,
      std::unordered_map<std::string, std::string> const& headerFields,
      bool raw);

  v8::Handle<v8::Value> postData(
      v8::Isolate* isolate, std::string_view location,
      v8::Local<v8::Value> const& body,
      std::unordered_map<std::string, std::string> const& headerFields,
      bool raw = false, bool isFile = false);

  v8::Handle<v8::Value> putData(
      v8::Isolate* isolate, std::string_view location,
      v8::Local<v8::Value> const& body,
      std::unordered_map<std::string, std::string> const& headerFields,
      bool raw);

  v8::Handle<v8::Value> patchData(
      v8::Isolate* isolate, std::string_view location,
      v8::Local<v8::Value> const& body,
      std::unordered_map<std::string, std::string> const& headerFields,
      bool raw);

  std::string getHandle();

  void connectHandle(v8::Isolate* isolate,
                     v8::FunctionCallbackInfo<v8::Value> const& args,
                     std::string const& handle);

  void disconnectHandle(v8::Isolate* isolate,
                        v8::FunctionCallbackInfo<v8::Value> const& args,
                        std::string const& handle);

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  uint32_t sendFuzzRequest(fuzzer::RequestFuzzer& fuzzer);
#endif

  // forces a new connection to be used
  void forceNewConnection();

  void initServer(v8::Isolate*, v8::Handle<v8::Context> context);

 private:
  std::shared_ptr<fuerte::Connection> createConnection(
      bool bypassCache = false);
  std::shared_ptr<fuerte::Connection> acquireConnection();

  v8::Local<v8::Value> requestData(
      v8::Isolate* isolate, fuerte::RestVerb verb, std::string_view location,
      v8::Local<v8::Value> const& body,
      std::unordered_map<std::string, std::string> const& headerFields,
      bool isFile = false);

  v8::Local<v8::Value> requestDataRaw(
      v8::Isolate* isolate, fuerte::RestVerb verb, std::string_view location,
      v8::Local<v8::Value> const& body,
      std::unordered_map<std::string, std::string> const& headerFields);

  v8::Local<v8::Value> handleResult(v8::Isolate* isolate,
                                    std::unique_ptr<fuerte::Response> response,
                                    fuerte::Error ec);

  /// @brief shuts down the connection _connection and resets the pointer
  /// to a nullptr
  void shutdownConnection();

  void setCustomError(int httpCode, std::string const& msg) {
    _setCustomError = true;
    _lastHttpReturnCode = httpCode;
    _lastErrorMessage = msg;
  }

 private:
  ArangoshServer& _server;
  ClientFeature& _client;

  std::string _databaseName;
  std::chrono::duration<double> _requestTimeout;

  mutable std::recursive_mutex _lock;
  unsigned _lastHttpReturnCode;
  std::string _lastErrorMessage;
  std::string _version;
  std::string _mode;
  std::string _role;

  fuerte::EventLoopService _loop;
  fuerte::ConnectionBuilder _builder;
  fuerte::ConnectionBuilder _connectedBuilder;
  std::string _currentConnectionId;
  std::shared_ptr<fuerte::Connection> _connection;
  velocypack::Options _vpackOptions;
  bool _forceJson;
  std::atomic<bool> _setCustomError;

  // a per-endpoint, per-user cache for connections. whenever we reconnect
  // to another endpoint, we can put the old connection into this cache,
  // and recycle it later. the goal is to not use too many separate connections
  // and ephemeral ports for patterns such as "connect-to-leader" ->
  // "connect-to-follower"
  // -> "connect-to-leader" etc.
  std::unordered_map<std::string, std::shared_ptr<fuerte::Connection>>
      _connectionCache;
  std::unordered_map<std::string, fuerte::ConnectionBuilder>
      _connectionBuilderCache;
};
}  // namespace arangodb
