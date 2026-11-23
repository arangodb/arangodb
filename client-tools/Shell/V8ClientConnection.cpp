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

#include "V8ClientConnection.h"

#include "Utils/RequestCoding.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/FileUtils.h"
#include "Basics/EncodingUtils.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Logger/LogMacros.h"
#include "Rest/GeneralResponse.h"
#include "Rest/Version.h"
#include "Shell/ClientFeature.h"
#include "fuerte/types.h"
#ifdef ARANGODB_ENABLE_FAILURE_TESTS
#include "Shell/RequestFuzzer.h"
#endif
#include "Ssl/SslInterface.h"

#include <absl/strings/str_cat.h>
#include <fuerte/connection.h>
#include <fuerte/requests.h>
#include <fuerte/helper.h>
#include "Ssl/jwt.h"
#include <velocypack/Builder.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <filesystem>
#include <stdexcept>
#include <boost/hash2/md5.hpp>

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::request_coding;

namespace fu = arangodb::fuerte;

namespace {
// return an identifier to a connection configuration, consisting of
// endpoint, username, password, jwt, authentication and protocol type.
// the time claims of the jwt are left out, so that a renewed or regenerated
// token for the same identity keeps the identifier
std::string connectionIdentifier(fuerte::ConnectionBuilder const& builder) {
  std::string const jwtToken = builder.jwtToken();
  std::string const raw = absl::StrCat(
      builder.normalizedEndpoint(), "/", builder.user(), "/",
      builder.password(), "/",
      arangodb::rest::SslInterface::jwt::extractTimelessClaims(jwtToken)
          .value_or(jwtToken),
      "/", to_string(builder.authenticationType()), "/",
      to_string(builder.protocolType()));
  // create md5
  boost::hash2::md5_128 hash;
  hash.update(raw.c_str(), raw.size());
  std::string hexString{boost::hash2::to_string(hash.result())};

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  LOG_TOPIC("9aaaa", TRACE, arangodb::Logger::HTTPCLIENT)
      << "Connection identifier " << hexString << " calculated from: " << raw;
#endif
  // and return
  return hexString;
}

}  // namespace

V8ClientConnection::V8ClientConnection(
    application_features::ApplicationServer& server, ClientFeature& client)
    : _server(server),
      _client(client),
      _requestTimeout(_client.requestTimeout()),
      _lastHttpReturnCode(0),
      _lastErrorMessage(""),
      _version("arango"),
      _mode("unknown mode"),
      _role("UNKNOWN"),
      _loop(1, "V8ClientConnection"),
      _foundConnectionClose(false),
      _forceJson(false),
      _setCustomError(false) {
  _handles.config().builder.maxConnectRetries(3);
  _handles.config().builder.connectRetryPause(std::chrono::milliseconds(100));
  _handles.config().builder.connectTimeout(std::chrono::milliseconds(
      static_cast<int64_t>(1000.0 * _client.connectionTimeout())));
  _handles.config().builder.onFailure([this](fu::Error err,
                                             std::string const& msg) {
    // care only about connection errors
    if (err == fu::Error::CouldNotConnect || err == fu::Error::ProtocolError) {
      std::unique_lock<std::recursive_mutex> guard(_lock, std::try_to_lock);
      if (guard && !_setCustomError) {
        _lastHttpReturnCode = 503;
        _lastErrorMessage = msg;
      }
      _setCustomError = false;
    }
  });
}

V8ClientConnection::~V8ClientConnection() {
  _handles.config().builder.onFailure(nullptr);  // reset callback
  shutdownConnection();
  _loop.stop();
}

std::shared_ptr<fu::Connection> V8ClientConnection::createConnection(
    bool bypassCache) {
  if (endpoint() == "none") {
    setCustomError(400, "no endpoint specified");
    return nullptr;
  }

  // try to find an existing connection in the cache
  // the cache has one connection per endpoint. the active connection is only
  // replaced once the new one has proven to work
  std::lock_guard<std::recursive_mutex> guard(_lock);
  std::string const id = connectionIdentifier(_handles.config().builder);
  Handles::Entry candidate{nullptr, _handles.config()};
  bool wasFromCache = false;
  if (!bypassCache) {
    if (auto cached = _handles.take(id); cached.has_value()) {
      if (cached->config.builder.jwtToken() ==
          _handles.config().builder.jwtToken()) {
        // reuse the connection, but keep the new configuration: it holds
        // the endpoint and username as the client gave them this time
        candidate.connection = std::move(cached->connection);
        wasFromCache = true;
      } else if (cached->connection != nullptr) {
        // same identity, but a newer token. a connection keeps the token it
        // was created with, so the cached one is replaced
        cached->connection->cancel();
      }
    }
  }
  if (candidate.connection == nullptr) {
    candidate.connection = candidate.config.builder.connect(_loop);
  }
  auto newConnection = candidate.connection;
  int retryCount = wasFromCache ? 2 : 1;
  fu::StringMap params{{"details", "true"}};
  while (retryCount > 0) {
    auto req = fu::createRequest(fu::RestVerb::Get, "/_api/version", params);
    if (_forceJson) {
      req->header.acceptType(fu::ContentType::Json);
    }
    req->header.database = candidate.config.databaseName;
    req->timeout(std::chrono::seconds(30));
    retryCount -= 1;
    try {
      auto res = newConnection->sendRequest(std::move(req));

      if (!res) {
        setCustomError(500, "unable to create connection");
        LOG_TOPIC("9daaa", DEBUG, arangodb::Logger::HTTPCLIENT)
            << "Connection attempt to endpoint '" << endpoint()
            << "' failed fatally from: " << newConnection->localEndpoint();
        return nullptr;
      }

      _lastHttpReturnCode = res->statusCode();

      std::shared_ptr<VPackBuilder> parsedBody;
      VPackSlice body;
      if (res->contentType() == fu::ContentType::VPack) {
        body = res->slice();
      } else if (res->contentType() == fu::ContentType::Json) {
        parsedBody = VPackParser::fromJson(
            reinterpret_cast<char const*>(res->payload().data()),
            res->payload().size());
        body = parsedBody->slice();
      }
      if (_lastHttpReturnCode >= 400) {
        auto const& headers = res->messageHeader().meta();
        auto it = headers.find("http/1.1");
        if (it != headers.end()) {
          std::string errorMessage = (*it).second;
          if (body.isObject()) {
            std::string const msg = VelocyPackHelper::getStringValue(
                body, StaticStrings::ErrorMessage, "");
            if (!msg.empty()) {
              errorMessage = msg;
            }
          }
          setCustomError(_lastHttpReturnCode, errorMessage);
          LOG_TOPIC("9daab", DEBUG, arangodb::Logger::HTTPCLIENT)
              << "Connection attempt to endpoint '" << endpoint()
              << "' failed: " << errorMessage
              << " from: " << newConnection->localEndpoint();
          return nullptr;
        }
      }

      if (!body.isObject()) {
        std::string msg("invalid response: '");
        msg += std::string(reinterpret_cast<char const*>(res->payload().data()),
                           res->payload().size());
        msg += "'";
        setCustomError(503, msg);
        LOG_TOPIC("9daac", DEBUG, arangodb::Logger::HTTPCLIENT)
            << "Connection attempt to endpoint '" << endpoint()
            << "' failed: " << msg
            << " from: " << newConnection->localEndpoint();
        return nullptr;
      }

      // from here on, newConnection is the active connection
      _handles.activate(id, std::move(candidate));

      std::string const server =
          VelocyPackHelper::getStringValue(body, "server", "");

      // "server" value is a string and content is "arango"
      if (server == "arango") {
        // look up "version" value
        _version = VelocyPackHelper::getStringValue(body, "version", "");
        VPackSlice const details = body.get("details");
        if (details.isObject()) {
          VPackSlice const mode = details.get("mode");
          if (mode.isString()) {
            _mode = mode.copyString();
          }
          VPackSlice role = details.get("role");
          if (role.isString()) {
            _role = role.copyString();
          }
        }
        if (!body.hasKey("version")) {
          // if we don't get a version number in return, the server is
          // probably running in hardened mode
          return newConnection;
        }
        std::string const versionString =
            VelocyPackHelper::getStringValue(body, "version", "");
        std::pair<int, int> version =
            rest::Version::parseVersionString(versionString);
        if (version.first < 3) {
          // major version of server is too low
          //_client->disconnect();
          shutdownConnection();
          std::string msg =
              absl::StrCat("Server version number ('", versionString,
                           "') is too low. Expecting 3.0 or higher");
          setCustomError(500, msg);
          return newConnection;
        }
      }
      return newConnection;
    } catch (fu::Error const& e) {  // connection error
      if (retryCount <= 0) {
        std::string msg(fu::to_string(e));
        setCustomError(503, msg);
        LOG_TOPIC("9daad", DEBUG, arangodb::Logger::HTTPCLIENT)
            << "Connection attempt to endpoint '" << endpoint()
            << "' failed: " << msg
            << " from: " << newConnection->localEndpoint();
        return nullptr;
      } else {
        newConnection->cancel();
        newConnection = candidate.config.builder.connect(_loop);
        candidate.connection = newConnection;
      }
    }
  }
  return nullptr;
}

std::shared_ptr<fu::Connection> V8ClientConnection::acquireConnection(
    bool bypassCache) {
  std::lock_guard<std::recursive_mutex> guard(_lock);

  _lastErrorMessage.clear();
  _lastHttpReturnCode = 0;

  // renew the token before sending a request with it
  renewTokenIfDue();

  if (!_handles.connection() ||
      (_handles.connection()->state() == fu::Connection::State::Closed) ||
      _foundConnectionClose) {
    _foundConnectionClose = false;
    return createConnection(bypassCache);
  }
  return _handles.connection();
}

void V8ClientConnection::setInterrupted(bool interrupted) {
  std::lock_guard<std::recursive_mutex> guard(_lock);
  if (interrupted && _handles.connection() != nullptr) {
    shutdownConnection();
  } else if (!interrupted && (_handles.connection() == nullptr ||
                              (_handles.connection()->state() ==
                               fu::Connection::State::Closed))) {
    createConnection();
  }
}

bool V8ClientConnection::isConnected() const {
  std::lock_guard<std::recursive_mutex> guard(_lock);
  if (_handles.connection()) {
    if (_handles.connection()->state() == fu::Connection::State::Connected) {
      return true;
    }
    // the client might have automatically closed the connection,
    // as long as there was no error we can reconnect
    return _lastHttpReturnCode < 400;
  }
  return false;
}

std::string V8ClientConnection::endpointSpecification() const {
  std::lock_guard<std::recursive_mutex> guard(_lock);
  if (_handles.connection()) {
    return _handles.connection()->endpoint();
  }
  return "";
}

application_features::ApplicationServer& V8ClientConnection::server() {
  return _server;
}

std::string V8ClientConnection::databaseName() const {
  std::lock_guard<std::recursive_mutex> guard(_lock);
  return _handles.config().databaseName;
}

void V8ClientConnection::setDatabaseName(std::string const& value) {
  std::lock_guard<std::recursive_mutex> guard(_lock);
  _handles.config().databaseName = value;
}

double V8ClientConnection::timeout() const { return _requestTimeout.count(); }

void V8ClientConnection::timeout(double value) {
  _requestTimeout = std::chrono::duration<double>(value);
}

std::string V8ClientConnection::endpoint() const {
  std::lock_guard<std::recursive_mutex> guard(_lock);
  return _handles.config().endpoint.empty() ? _client.endpoint()
                                            : _handles.config().endpoint;
}

std::string V8ClientConnection::username() const {
  std::lock_guard<std::recursive_mutex> guard(_lock);
  return _handles.config().endpoint.empty() ? _client.username()
                                            : _handles.config().username;
}

std::string V8ClientConnection::password() const {
  std::lock_guard<std::recursive_mutex> guard(_lock);
  return _handles.config().endpoint.empty() ? _client.password()
                                            : _handles.config().password;
}

std::string V8ClientConnection::protocol() const {
  switch (_handles.config().builder.protocolType()) {
    case fuerte::ProtocolType::Http:
      return "http";
    case fuerte::ProtocolType::Http2:
      return "http2";
    default:
      return "unknown";
  }
}

// Helper function to authenticate via /_open/auth endpoint
TokenOutcome V8ClientConnection::requestToken(
    TokenRequest request, fu::ConnectionBuilder const& target,
    std::string const& username, std::string const& password) {
  try {
    std::shared_ptr<fu::Connection> connection;
    if (request == TokenRequest::kRenew && _handles.connection() != nullptr &&
        _handles.connection()->state() == fu::Connection::State::Connected &&
        _handles.config().builder.jwtToken() == target.jwtToken()) {
      // the active connection already authenticates with the token
      connection = _handles.connection();
    } else {
      // a temporary connection to the endpoint of target and with its
      // settings, which does not report failures as errors of this
      // connection. a login sends credentials instead of a token
      fu::ConnectionBuilder tempBuilder = target;
      tempBuilder.onFailure(nullptr);
      if (request == TokenRequest::kLogin) {
        tempBuilder.user("").password("").jwtToken("").authenticationType(
            fu::AuthenticationType::None);
      }
      connection = tempBuilder.connect(_loop);
    }
    if (!connection) {
      return TokenOutcome::error(
          TRI_ERROR_SIMPLE_CLIENT_COULD_NOT_CONNECT,
          "Failed to create connection for authentication");
    }
    auto req = std::make_unique<fu::Request>();
    req->header.restVerb = fu::RestVerb::Post;
    req->header.path =
        request == TokenRequest::kLogin ? "/_open/auth" : "/_open/auth/renew";
    req->header.contentType(fu::ContentType::Json);
    req->header.acceptType(fu::ContentType::Json);
    req->timeout(
        std::chrono::duration_cast<std::chrono::milliseconds>(_requestTimeout));

    if (request == TokenRequest::kLogin) {
      velocypack::Builder bodyBuilder;
      bodyBuilder.openObject();
      bodyBuilder.add("username", username);
      bodyBuilder.add("password", password);
      bodyBuilder.close();
      std::string jsonBody = bodyBuilder.slice().toJson();
      req->addBinary(reinterpret_cast<uint8_t const*>(jsonBody.data()),
                     jsonBody.size());
    }

    // Send the request
    auto response = connection->sendRequest(std::move(req));
    if (!response) {
      return TokenOutcome::error(TRI_ERROR_FAILED,
                                 "Failed to send authentication request");
    }
    std::shared_ptr<VPackBuilder> parsedBody;
    VPackSlice body;
    if (response->payloadSize() > 0) {
      try {
        parsedBody = VPackParser::fromJson(
            reinterpret_cast<char const*>(response->payload().data()),
            response->payload().size());
        body = parsedBody->slice();
      } catch (std::exception const&) {
        // not JSON; body stays None
      }
    }
    return parseTokenReply(request, static_cast<int>(response->statusCode()),
                           body);
  } catch (std::exception const& ex) {
    return TokenOutcome::error(TRI_ERROR_FAILED, ex.what());
  } catch (fu::Error const& ec) {
    return TokenOutcome::error(TRI_ERROR_FAILED, fuerte::v1::to_string(ec));
  } catch (...) {
    // intentional fall through to error
  }
  return TokenOutcome::error(TRI_ERROR_FAILED, "unknown error");
}

void V8ClientConnection::renewTokenIfDue() {
  std::lock_guard<std::recursive_mutex> guard(_lock);
  std::string newToken;
  switch (_handles.config().renewal.kind) {
    case TokenRenewal::Kind::kNone:
      return;
    case TokenRenewal::Kind::kJwtTokenOption:
      // the ClientFeature renews the --server.jwt-token in the background, at
      // its endpoint, which adoptClientSettings() keeps at the active handle's
      newToken = _client.jwtToken();
      break;
    case TokenRenewal::Kind::kLogin: {
      auto const now = JwtClock::now();
      auto const threshold = std::chrono::duration_cast<JwtClock::duration>(
          std::chrono::duration<double>{_client.jwtRenewalThreshold()});
      if (!isRenewalDue(_handles.config().renewal.token, now, threshold)) {
        return;
      }
      // both at the server of this connection, as the user it was created for
      auto const& config = _handles.config();
      auto const outcome = renewOrLogin(
          config.renewal.token, now,
          [&] { return requestToken(TokenRequest::kRenew, config.builder); },
          [&] {
            return requestToken(TokenRequest::kLogin, config.builder,
                                config.username, config.password);
          });
      if (outcome.fail()) {
        LOG_TOPIC("7552f", WARN, arangodb::Logger::AUTHENTICATION)
            << "renewing the JWT token of user '" << _handles.config().username
            << "' at '" << _handles.config().endpoint
            << "' failed: " << outcome.errorMessage() << "; retrying in "
            << kRenewalRetryInterval.count() << " seconds at the earliest";
      }
      _handles.config().renewal.token = applyRenewal(
          std::move(_handles.config().renewal.token), outcome, JwtClock::now());
      newToken = _handles.config().renewal.token.token;
      break;
    }
  }
  if (newToken.empty() || newToken == _handles.config().builder.jwtToken()) {
    return;
  }
  // a connection keeps the token it was created with, so switch to a new
  // one. its identifier stays the same, so it supersedes the old connection
  _handles.config().builder.jwtToken(newToken);
  if (_handles.connection() != nullptr) {
    shutdownConnection();
    createConnection();
  }
}

std::string const& V8ClientConnection::internalToken(
    std::string const& jwtSecret) {
  // superuser tokens do not expire, and a new one would only differ in its
  // "iat" claim. reusing it lets a reconnect reuse the cached connection,
  // which keeps the token it was created with
  if (_internalToken.empty() || _internalTokenSecret != jwtSecret) {
    _internalToken = arangodb::rest::SslInterface::jwt::generateInternalToken(
        jwtSecret, "arangosh");
    _internalTokenSecret = jwtSecret;
  }
  return _internalToken;
}

ResultT<std::string> V8ClientConnection::prepareConnection() {
  // Need to hold _lock when running this function
  _forceJson = _client.forceJson();
  _requestTimeout = std::chrono::duration<double>(_client.requestTimeout());

  // configure a copy, so that a failed login leaves the configuration
  // untouched. each authentication mode clears the fields of the others;
  // leftovers would become part of the connection identifier
  fu::ConnectionBuilder builder = _handles.config().builder;
  builder.endpoint(_client.endpoint());
  TokenRenewal renewal;
  auto const useJwt = [&builder](std::string const& token) {
    builder.jwtToken(token).user("").password("").authenticationType(
        fu::AuthenticationType::Jwt);
  };

  // check jwtToken first, then jwtSecret, as they are empty by default,
  // but username defaults to "root" in most configurations
  TRI_ASSERT(_client.jwtToken().empty() || _client.jwtSecret().empty());

  if (!_client.jwtToken().empty()) {
    useJwt(_client.jwtToken());
    renewal.kind = TokenRenewal::Kind::kJwtTokenOption;
  } else if (!_client.jwtSecret().empty()) {
    useJwt(internalToken(_client.jwtSecret()));
  } else if (!_client.username().empty()) {
    // Use new authentication method via /_open/auth endpoint
    auto const login = requestToken(TokenRequest::kLogin, builder,
                                    _client.username(), _client.password());
    if (login.fail()) {
      return ResultT<std::string>::error(login.errorNumber(),
                                         login.errorMessage());
    }
    if (auto const& token = login.get(); token.has_value()) {
      // Server has authentication enabled, use the JWT token and renew it
      // with the credentials of the configuration
      useJwt(*token);
      renewal = TokenRenewal{.kind = TokenRenewal::Kind::kLogin,
                             .token = tokenStateFor(*token, JwtClock::now())};
    } else {
      // the server does not implement the _open/auth API, or has
      // authentication disabled and issues no real token: use basic auth
      builder.user(_client.username())
          .password(_client.password())
          .jwtToken("")
          .authenticationType(fu::AuthenticationType::Basic);
    }
  } else {
    builder.user("").password("").jwtToken("").authenticationType(
        fu::AuthenticationType::None);
  }
  // the configuration describes the active connection, so put that into the
  // cache for later reuse before retargeting it
  _handles.deactivate();
  _handles.config() = Configuration{.builder = std::move(builder),
                                    .renewal = std::move(renewal),
                                    .endpoint = _client.endpoint(),
                                    .username = _client.username(),
                                    .password = _client.password(),
                                    .jwtSecret = _client.jwtSecret(),
                                    .databaseName = _client.databaseName()};
  return ResultT<std::string>::success("");
}

void V8ClientConnection::adoptClientSettings(Configuration const& config) {
  _client.setEndpoint(config.endpoint);
  _client.setUsername(config.username);
  _client.setPassword(config.password);
  _client.setJwtSecret(config.jwtSecret);
  _client.setDatabaseName(config.databaseName);
}

ResultT<std::string> V8ClientConnection::connect() {
  std::lock_guard<std::recursive_mutex> guard(_lock);
  auto res = prepareConnection();
  if (res.ok()) {
    createConnection();
  }
  return res;
}

ResultT<std::string> V8ClientConnection::reconnect() {
  std::lock_guard<std::recursive_mutex> guard(_lock);

  auto res = prepareConnection();
  if (!res.ok()) {
    // the caller already put the failed settings into the client. before the
    // first connect there is no configuration to go back to
    if (!_handles.config().endpoint.empty()) {
      adoptClientSettings(_handles.config());
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

std::string V8ClientConnection::getHandle() { return _handles.activeHandle(); }

std::vector<V8ClientConnection::HandleInfo>
V8ClientConnection::connectionHandleTable() const {
  std::lock_guard<std::recursive_mutex> guard(_lock);
  std::vector<HandleInfo> table;
  auto const addEntry = [&](std::string const& id,
                            std::shared_ptr<fu::Connection> const& conn,
                            Configuration const& config, bool isActive) {
    std::optional<HandleInfo::Peer> peer;
    if (conn) {
      peer = HandleInfo::Peer{
          .connected = conn->state() == fu::Connection::State::Connected,
          .endpoint = conn->endpoint(),
          .localEndpoint = conn->localEndpoint()};
    }
    table.push_back(HandleInfo{.handle = id,
                               .active = isActive,
                               .peer = std::move(peer),
                               .username = config.username,
                               .password = config.password,
                               .jwtToken = config.builder.jwtToken()});
  };

  for (auto const& [id, cached] : _handles.inactive()) {
    addEntry(id, cached.connection, cached.config, /*isActive=*/false);
  }

  if (_handles.connection() != nullptr) {
    addEntry(_handles.activeHandle(), _handles.connection(), _handles.config(),
             /*isActive=*/true);
  }
  return table;
}

V8ClientConnection::HandleSwitch V8ClientConnection::connectHandle(
    std::string const& handle) {
#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  LOG_TOPIC("8aaaa", TRACE, arangodb::Logger::HTTPCLIENT)
      << "Connecting to handle: " << handle;
#endif
  std::lock_guard<std::recursive_mutex> guard(_lock);
  if (_handles.activeHandle() == handle) {
    return HandleSwitch::kAlreadyActive;
  }
  auto cached = _handles.take(handle);
  if (!cached.has_value()) {
    return HandleSwitch::kUnknownHandle;
  }
  // it brings its own renewal state, so an expired token is renewed on the
  // next request
  _handles.activate(handle, std::move(*cached));
  adoptClientSettings(_handles.config());
  return HandleSwitch::kSwitched;
}

bool V8ClientConnection::disconnectHandle(std::string const& handle) {
  std::lock_guard<std::recursive_mutex> guard(_lock);
  if (_handles.take(handle).has_value()) {
    return true;
  }
  if (!_handles.activeHandle().empty() && _handles.activeHandle() == handle) {
    _handles.dropActive();
    return true;
  }
  return false;
}

void V8ClientConnection::flushConnectionCache() {
  std::lock_guard<std::recursive_mutex> guard(_lock);
  _handles.clear();
}

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
void V8ClientConnection::reconnectWithNewPassword(std::string const& password) {
  _client.setPassword(password);
  this->reconnect();
}
#endif


#ifdef ARANGODB_ENABLE_FAILURE_TESTS
uint32_t V8ClientConnection::sendFuzzRequest(fuzzer::RequestFuzzer& fuzzer) {
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

ResultT<V8ClientConnection::RequestOutcome> V8ClientConnection::sendRequest(
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
void V8ClientConnection::forceNewConnection() {
  std::lock_guard<std::recursive_mutex> guard(_lock);

  _lastErrorMessage.clear();
  _lastHttpReturnCode = 0;

  // createConnection will populate the active connection
  createConnection(/*bypassCache*/ true);
}

void V8ClientConnection::shutdownConnection() {
  std::lock_guard<std::recursive_mutex> guard(_lock);
  if (_handles.connection()) {
    _handles.connection()->cancel();
  }
}
