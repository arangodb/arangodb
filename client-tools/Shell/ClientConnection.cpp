///////////////////////////////////////////////////////////////////////////////
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

#include "Shell/ClientConnection.h"

#include "Utils/RequestCoding.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Logger/LogMacros.h"
#include "Rest/GeneralResponse.h"
#include "Shell/ClientFeature.h"
#ifdef ARANGODB_ENABLE_FAILURE_TESTS
#include "Shell/RequestFuzzer.h"
#endif

#include <fuerte/connection.h>
#include <fuerte/helper.h>
#include <fuerte/requests.h>

using namespace arangodb;
using namespace arangodb::request_coding;

namespace fu = arangodb::fuerte;

ClientConnection::ClientConnection(
    application_features::ApplicationServer& server, ClientFeature& client)
    : _server(server),
      _client(client),
      _requestTimeout(_client.requestTimeout()),
      _lastHttpReturnCode(0),
      _lastErrorMessage(""),
      _version("arango"),
      _mode("unknown mode"),
      _role("UNKNOWN"),
      _loop(1, "ClientConnection"),
      _foundConnectionClose(false),
      _forceJson(false),
      _setCustomError(false),
      _manager(
          [this] {
            fu::ConnectionBuilder builder;
            builder.maxConnectRetries(3);
            builder.connectRetryPause(std::chrono::milliseconds(100));
            builder.connectTimeout(std::chrono::milliseconds(
                static_cast<int64_t>(1000.0 * _client.connectionTimeout())));
            builder.onFailure([this](fu::Error err, std::string const& msg) {
              // care only about connection errors
              if (err == fu::Error::CouldNotConnect ||
                  err == fu::Error::ProtocolError) {
                std::unique_lock<std::recursive_mutex> guard(_lock,
                                                             std::try_to_lock);
                if (guard && !_setCustomError) {
                  _lastHttpReturnCode = 503;
                  _lastErrorMessage = msg;
                }
                _setCustomError = false;
              }
            });
            return builder;
          }(),
          [this](fu::ConnectionBuilder builder) {
            return builder.connect(_loop);
          },
          &JwtClock::now) {}

ClientConnection::~ClientConnection() {
  shutdownConnection();
  _loop.stop();
}

std::shared_ptr<fu::Connection> ClientConnection::createConnection(
    bool bypassCache) {
  if (endpoint() == "none") {
    setCustomError(400, "no endpoint specified");
    return nullptr;
  }

  std::lock_guard<std::recursive_mutex> guard(_lock);
  auto outcome = _manager.connect(bypassCache, _forceJson);
  if (outcome.server.has_value()) {
    _version = outcome.server->version;
    _mode = outcome.server->mode.value_or(_mode);
    _role = outcome.server->role.value_or(_role);
  }
  if (outcome.errorMessage.has_value()) {
    setCustomError(outcome.httpCode, *outcome.errorMessage);
  } else {
    _lastHttpReturnCode = outcome.httpCode;
  }
  return std::move(outcome.connection);
}

std::shared_ptr<fu::Connection> ClientConnection::acquireConnection(
    bool bypassCache) {
  std::lock_guard<std::recursive_mutex> guard(_lock);

  _lastErrorMessage.clear();
  _lastHttpReturnCode = 0;

  // renew the token before sending a request with it
  renewTokenIfDue();

  if (!_manager.activeConnection() ||
      (_manager.activeConnection()->state() == fu::Connection::State::Closed) ||
      _foundConnectionClose) {
    _foundConnectionClose = false;
    return createConnection(bypassCache);
  }
  return _manager.activeConnection();
}

void ClientConnection::setInterrupted(bool interrupted) {
  std::lock_guard<std::recursive_mutex> guard(_lock);
  if (interrupted && _manager.activeConnection() != nullptr) {
    shutdownConnection();
  } else if (!interrupted && (_manager.activeConnection() == nullptr ||
                              (_manager.activeConnection()->state() ==
                               fu::Connection::State::Closed))) {
    createConnection();
  }
}

bool ClientConnection::isConnected() const {
  std::lock_guard<std::recursive_mutex> guard(_lock);
  if (_manager.activeConnection()) {
    if (_manager.activeConnection()->state() ==
        fu::Connection::State::Connected) {
      return true;
    }
    // the client might have automatically closed the connection,
    // as long as there was no error we can reconnect
    return _lastHttpReturnCode < 400;
  }
  return false;
}

std::string ClientConnection::endpointSpecification() const {
  std::lock_guard<std::recursive_mutex> guard(_lock);
  if (_manager.activeConnection()) {
    return _manager.activeConnection()->endpoint();
  }
  return "";
}

application_features::ApplicationServer& ClientConnection::server() {
  return _server;
}

std::string ClientConnection::databaseName() const {
  std::lock_guard<std::recursive_mutex> guard(_lock);
  return _manager.settings().databaseName;
}

void ClientConnection::setDatabaseName(std::string const& value) {
  std::lock_guard<std::recursive_mutex> guard(_lock);
  _manager.setDatabaseName(value);
}

double ClientConnection::timeout() const { return _requestTimeout.count(); }

void ClientConnection::timeout(double value) {
  _requestTimeout = std::chrono::duration<double>(value);
}

std::string ClientConnection::endpoint() const {
  std::lock_guard<std::recursive_mutex> guard(_lock);
  auto const& settings = _manager.settings();
  return settings.endpoint.empty() ? _client.endpoint() : settings.endpoint;
}

std::string ClientConnection::username() const {
  std::lock_guard<std::recursive_mutex> guard(_lock);
  auto const& settings = _manager.settings();
  return settings.endpoint.empty() ? _client.username() : settings.username;
}

std::string ClientConnection::password() const {
  std::lock_guard<std::recursive_mutex> guard(_lock);
  auto const& settings = _manager.settings();
  return settings.endpoint.empty() ? _client.password() : settings.password;
}

std::string ClientConnection::protocol() const {
  std::lock_guard<std::recursive_mutex> guard(_lock);
  switch (_manager.protocolType()) {
    case fuerte::ProtocolType::Http:
      return "http";
    case fuerte::ProtocolType::Http2:
      return "http2";
    default:
      return "unknown";
  }
}

std::chrono::milliseconds ClientConnection::requestTimeout() const {
  return std::chrono::duration_cast<std::chrono::milliseconds>(_requestTimeout);
}

void ClientConnection::renewTokenIfDue() {
  std::lock_guard<std::recursive_mutex> guard(_lock);
  auto const threshold = std::chrono::duration_cast<JwtClock::duration>(
      std::chrono::duration<double>{_client.jwtRenewalThreshold()});
  // the ClientFeature renews the --server.jwt-token in the background, at its
  // endpoint, which adoptClientSettings() keeps at the active handle's
  if (_manager.renewTokenIfDue(threshold, _client.jwtToken(),
                               requestTimeout()) &&
      _manager.activeConnection() != nullptr) {
    // a connection keeps the token it was created with
    shutdownConnection();
    createConnection();
  }
}

ResultT<std::string> ClientConnection::prepareConnection() {
  std::lock_guard<std::recursive_mutex> guard(_lock);
  _forceJson = _client.forceJson();
  _requestTimeout = std::chrono::duration<double>(_client.requestTimeout());
  auto configured = _manager.configure(
      ConnectionSettings{.endpoint = _client.endpoint(),
                         .username = _client.username(),
                         .password = _client.password(),
                         .jwtToken = _client.jwtToken(),
                         .jwtSecret = _client.jwtSecret(),
                         .databaseName = _client.databaseName()},
      requestTimeout());
  if (configured.fail()) {
    return ResultT<std::string>::error(std::move(configured));
  }
  return ResultT<std::string>::success("");
}

void ClientConnection::adoptClientSettings(ConnectionSettings const& settings) {
  _client.setEndpoint(settings.endpoint);
  _client.setUsername(settings.username);
  _client.setPassword(settings.password);
  _client.setJwtSecret(settings.jwtSecret);
  _client.setDatabaseName(settings.databaseName);
}

ResultT<std::string> ClientConnection::connect() {
  std::lock_guard<std::recursive_mutex> guard(_lock);
  auto res = prepareConnection();
  if (res.ok()) {
    createConnection();
  }
  return res;
}

ResultT<std::string> ClientConnection::reconnect() {
  std::lock_guard<std::recursive_mutex> guard(_lock);

  auto res = prepareConnection();
  if (!res.ok()) {
    // the caller already put the failed settings into the client. before the
    // first connect there is no configuration to go back to
    if (!_manager.settings().endpoint.empty()) {
      adoptClientSettings(_manager.settings());
    }
    return res;
  }

  try {
    createConnection();
  } catch (...) {
    return ResultT<std::string>::error(TRI_ERROR_FAILED,
                                       "error in '" + endpoint() + "'");
  }

  if (isConnected() &&
      _lastHttpReturnCode == static_cast<int>(rest::ResponseCode::OK)) {
    LOG_TOPIC("2d416", INFO, arangodb::Logger::HTTPCLIENT)
        << ClientFeature::buildConnectedMessage(
               endpointSpecification(), _version, _role, _mode, databaseName(),
               _client.username());
  } else {
    if (_client.getWarnConnect()) {
      LOG_TOPIC("9d7ea", ERR, arangodb::Logger::HTTPCLIENT)
          << "Could not connect to endpoint '" << endpoint() << "', username: '"
          << _client.username() << "' - Server message: " << _lastErrorMessage;
    }
    return ResultT<std::string>::error(
        TRI_ERROR_FAILED,
        !_lastErrorMessage.empty() ? _lastErrorMessage : "could not connect");
  }
  return ResultT<std::string>::success("");
}

std::string ClientConnection::getHandle() {
  std::lock_guard<std::recursive_mutex> guard(_lock);
  return _manager.activeHandle();
}

std::vector<ClientConnection::HandleInfo>
ClientConnection::connectionHandleTable() const {
  std::lock_guard<std::recursive_mutex> guard(_lock);
  return _manager.handleTable();
}

ClientConnection::HandleSwitch ClientConnection::connectHandle(
    std::string const& handle) {
  std::lock_guard<std::recursive_mutex> guard(_lock);
  auto const outcome = _manager.switchTo(handle);
  if (outcome == HandleSwitch::kSwitched) {
    adoptClientSettings(_manager.settings());
  }
  return outcome;
}

bool ClientConnection::disconnectHandle(std::string const& handle) {
  std::lock_guard<std::recursive_mutex> guard(_lock);
  return _manager.drop(handle);
}

void ClientConnection::flushConnectionCache() {
  std::lock_guard<std::recursive_mutex> guard(_lock);
  _manager.clear();
}

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
void ClientConnection::reconnectWithNewPassword(std::string const& password) {
  _client.setPassword(password);
  this->reconnect();
}
#endif


#ifdef ARANGODB_ENABLE_FAILURE_TESTS
uint32_t ClientConnection::sendFuzzRequest(fuzzer::RequestFuzzer& fuzzer) {
  std::shared_ptr<fu::Connection> connection = acquireConnection(true);
  if (!connection || connection->state() == fu::Connection::State::Closed) {
    return kFuzzNotConnected;
  }

  auto localEndpoint = getLocalEndpoint();
  auto req = fuzzer.createRequest();
  auto req_copy = *req;

  fu::Error rc = fu::Error::NoError;
  std::unique_ptr<fu::Response> response;
  try {
    response = connection->sendRequest(std::move(req));
  } catch (fu::Error const& ec) {
    rc = ec;
    if (rc != fu::Error::NoError) {
      LOG_TOPIC("39e53", INFO, arangodb::Logger::HTTPCLIENT)
          << "rc: " << static_cast<uint32_t>(rc)
          << " from: " << getLocalEndpoint();
    }
  }
  if (!connection || connection->state() == fu::Connection::State::Closed) {
    LOG_TOPIC("39e51", INFO, arangodb::Logger::HTTPCLIENT)
        << "connection closed after " << fuerte::v1::to_string(req_copy)
        << " state: " << to_string(connection->state())
        << " from: " << localEndpoint;
  }
  if (response) {
    LOG_TOPIC("39e52", INFO, arangodb::Logger::HTTPCLIENT)
        << " state: " << to_string(connection->state())
        << "Server response: " << fuerte::v1::to_string(*response);
    if (response->messageHeader().metaByKey("connection") == "Close") {
      _foundConnectionClose = true;
    }
  }

  if (rc == fu::Error::ConnectionClosed) {
    return kFuzzClosedConnectionCode;
  }

  // not complete
  if (!response) {
    return kFuzzNoResponseCode;
  }

  TRI_ASSERT(response != nullptr);

  // complete
  return response->statusCode();
}
#endif

ResultT<ClientConnection::RequestOutcome> ClientConnection::sendRequest(
    fu::RestVerb verb, std::string_view location,
    std::unordered_map<std::string, std::string> const& headerFields,
    RequestBody const& body, std::chrono::milliseconds timeout) {
  bool retry = true;
  while (true) {
    auto req = std::make_unique<fu::Request>();
    translateHeaders(*req, verb, location, databaseName(), _forceJson, timeout,
                     headerFields, _client.compressTransfer());
    if (auto res = addRequestBody(*req, body, _forceJson,
                                  _client.compressTransfer()
                                      ? _client.compressRequestThreshold()
                                      : 0);
        res.fail()) {
      return ResultT<RequestOutcome>::error(std::move(res));
    }

    std::shared_ptr<fu::Connection> connection = acquireConnection(false);
    if (!connection || connection->state() == fu::Connection::State::Closed) {
      return ResultT<RequestOutcome>::error(
          TRI_ERROR_SIMPLE_CLIENT_COULD_NOT_CONNECT, "not connected");
    }

    fu::Error rc = fu::Error::NoError;
    std::unique_ptr<fu::Response> response;
    try {
      response = connection->sendRequest(std::move(req));
    } catch (fu::Error const& ec) {
      rc = ec;
    }

    if (rc == fu::Error::ConnectionClosed && retry) {
      retry = false;
      continue;
    }

    if (!response) {
      _lastErrorMessage = fu::to_string(rc);
      _lastHttpReturnCode = static_cast<int>(rest::ResponseCode::SERVER_ERROR);
      return ResultT<RequestOutcome>::success(RequestOutcome{
          .response = nullptr,
          .error = Result{fuerteToArangoErrorCode(rc), _lastErrorMessage}});
    }

    _lastHttpReturnCode = response->statusCode();
    if (response->payloadSize() > 0) {
      if (auto code = uncompressResponse(*response);
          code != TRI_ERROR_NO_ERROR) {
        return ResultT<RequestOutcome>::error(
            code, "Error inflating compressed response body");
      }
    }
    return ResultT<RequestOutcome>::success(
        RequestOutcome{.response = std::move(response), .error = {}});
  }
}

// forces a new connection to be used
void ClientConnection::forceNewConnection() {
  std::lock_guard<std::recursive_mutex> guard(_lock);

  _lastErrorMessage.clear();
  _lastHttpReturnCode = 0;

  // createConnection will populate the active connection
  createConnection(/*bypassCache*/ true);
}

void ClientConnection::shutdownConnection() {
  std::lock_guard<std::recursive_mutex> guard(_lock);
  if (_manager.activeConnection()) {
    _manager.activeConnection()->cancel();
  }
}
