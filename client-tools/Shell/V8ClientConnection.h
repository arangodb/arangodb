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
#include <v8.h>

#include <memory>

#include "Basics/ResultT.h"
#include "Utils/ConnectionHandles.h"
#include "Utils/RenewingJwtToken.h"
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
  explicit V8ClientConnection(application_features::ApplicationServer&,
                              ClientFeature&);
  ~V8ClientConnection();

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

  std::string const& databaseName() const { return _databaseName; }
  void setDatabaseName(std::string const& value);
  void setForceJson(bool value) { _forceJson = value; };
  /// @brief endpoint and user of the active connection, as configured when
  /// it was created; those of the client before the first connect
  std::string endpoint() const;
  std::string username() const;
  std::string password() const { return _handles.config().builder.password(); }
  int lastHttpReturnCode() const { return _lastHttpReturnCode; }
  std::string lastErrorMessage() const { return _lastErrorMessage; }
  std::string const& version() const { return _version; }
  std::string const& mode() const { return _mode; }
  std::string const& role() const { return _role; }
  std::string endpointSpecification() const;

  std::string getLocalEndpoint() {
    if (auto const& connection = _handles.connection(); connection) {
      return connection->localEndpoint();
    } else {
      return "not connected";
    }
  }
  application_features::ApplicationServer& server();

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

  void getConnectionHandleTable(
      v8::Isolate* isolate, v8::FunctionCallbackInfo<v8::Value> const& args);

  void connectHandle(v8::Isolate* isolate,
                     v8::FunctionCallbackInfo<v8::Value> const& args,
                     std::string const& handle);

  void disconnectHandle(v8::Isolate* isolate,
                        v8::FunctionCallbackInfo<v8::Value> const& args,
                        std::string const& handle);
  void flushConnectionCache(v8::Isolate* isolate,
                            v8::FunctionCallbackInfo<v8::Value> const& args,
                            std::string const& handle);

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  uint32_t sendFuzzRequest(fuzzer::RequestFuzzer& fuzzer);
#endif

  // forces a new connection to be used
  void forceNewConnection();

  void initServer(v8::Isolate*, v8::Handle<v8::Context> context);

 private:
  // how the jwt token of a connection is kept fresh
  struct TokenRenewal {
    enum class Kind {
      // basic auth, no auth, or a token without expiry from --server.jwt-secret
      kNone,
      // a user token from /_open/auth, renewed by logging in again
      kLogin,
      // the --server.jwt-token, renewed by the ClientFeature in the background
      kJwtTokenOption,
    };
    Kind kind = Kind::kNone;
    // password and token state of kLogin; the user is that of the
    // Configuration
    std::string password;
    JwtTokenState token;
  };

  // how a connection is configured
  struct Configuration {
    fuerte::ConnectionBuilder builder;
    TokenRenewal renewal;
    // the endpoint and user as given by the client
    std::string endpoint;
    std::string username;
  };

  using Handles = ConnectionHandles<Configuration>;

  std::shared_ptr<fuerte::Connection> createConnection(
      bool bypassCache = false);
  std::shared_ptr<fuerte::Connection> acquireConnection(bool bypassCache);

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

  /// @brief cancels the active connection, which stays active but closed
  /// until it is replaced
  void shutdownConnection();

  void setCustomError(int httpCode, std::string const& msg) {
    _setCustomError = true;
    _lastHttpReturnCode = httpCode;
    _lastErrorMessage = msg;
  }

  // Requests a token at the endpoint of target: kLogin logs in via
  // /_open/auth with username and password, kRenew renews the token of target
  // via /_open/auth/renew, over the active connection if that has the token.
  // yields no token if the server issues none (yet). Need to hold _lock
  TokenOutcome requestToken(TokenRequest request,
                            fuerte::ConnectionBuilder const& target,
                            std::string const& username = "",
                            std::string const& password = "");

  // The superuser token for the jwt secret, generated once per secret
  std::string const& internalToken(std::string const& jwtSecret);

  // Renews the token of the active configuration if due, and switches to a
  // connection with the new token
  void renewTokenIfDue();

 private:
  application_features::ApplicationServer& _server;
  ClientFeature& _client;

  std::string _databaseName;

  // the last superuser token, and the jwt secret it was generated with
  std::string _internalToken;
  std::string _internalTokenSecret;
  std::chrono::duration<double> _requestTimeout;

  mutable std::recursive_mutex _lock;
  unsigned _lastHttpReturnCode;
  std::string _lastErrorMessage;
  std::string _version;
  std::string _mode;
  std::string _role;

  fuerte::EventLoopService _loop;
  bool _foundConnectionClose;
  velocypack::Options _vpackOptions;
  bool _forceJson;
  std::atomic<bool> _setCustomError;

  // the active connection and a per-endpoint, per-user cache of inactive
  // ones. whenever we reconnect to another endpoint, we can put the old
  // connection into this cache, and recycle it later. the goal is to not use
  // too many separate connections and ephemeral ports for patterns such as
  // "connect-to-leader" -> "connect-to-follower" -> "connect-to-leader" etc.
  // the config() of the active entry is the one to connect with next while no
  // connection is active; whoever retargets it to another connection has to
  // deactivate() the active connection first
  Handles _handles;
};
}  // namespace arangodb
