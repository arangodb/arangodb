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

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/FileUtils.h"
#include "Basics/EncodingUtils.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/system-functions.h"
#include "Import/ImportHelper.h"
#include "Logger/LogMacros.h"
#include "Rest/GeneralResponse.h"
#include "Rest/Version.h"
#include "Shell/ClientFeature.h"
#include "fuerte/types.h"
#ifdef ARANGODB_ENABLE_FAILURE_TESTS
#include "Shell/RequestFuzzer.h"
#endif
#include "Shell/ShellConsoleFeature.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "Ssl/SslInterface.h"
#include "Ssl/ssl-helper.h"
#include "Utilities/NameValidator.h"
#include "V8/V8SecurityFeature.h"
#include "V8/v8-buffer.h"
#include "V8/v8-conv.h"
#include "V8/v8-deadline.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"
#ifdef USE_ENTERPRISE
#include "Enterprise/Encryption/EncryptionFeature.h"
#endif

#include <absl/strings/str_cat.h>
#include <fuerte/connection.h>
#include <fuerte/requests.h>
#include <fuerte/helper.h>
#include "Ssl/jwt.h"
#include <v8.h>
#include <velocypack/Builder.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <stdexcept>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/hash2/md5.hpp>

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::httpclient;
using namespace arangodb::import;

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
      _vpackOptions(VPackOptions::Defaults),
      _forceJson(false),
      _setCustomError(false) {
  _vpackOptions.buildUnindexedObjects = true;
  _vpackOptions.buildUnindexedArrays = true;

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

void V8ClientConnection::getConnectionHandleTable(
    v8::Isolate* isolate, v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Object> table = v8::Object::New(isolate);

  std::lock_guard<std::recursive_mutex> guard(_lock);

  auto const setString = [&](char const* key, std::string const& value,
                             auto& entry) {
    entry
        ->Set(context, TRI_V8_ASCII_STRING(isolate, key),
              TRI_V8_STD_STRING(isolate, value))
        .FromMaybe(false);
  };

  auto const setBool = [&](char const* key, bool value, auto& entry) {
    entry
        ->Set(context, TRI_V8_ASCII_STRING(isolate, key),
              value ? v8::True(isolate) : v8::False(isolate))
        .FromMaybe(false);
  };

  auto const addEntry = [&](std::string const& id,
                            std::shared_ptr<fu::Connection> const& conn,
                            Configuration const& config, bool isActive) {
    v8::Local<v8::Object> entry = v8::Object::New(isolate);

    setBool("active", isActive, entry);
    if (conn) {
      setBool("connected", conn->state() == fu::Connection::State::Connected,
              entry);
      setString("endpoint", conn->endpoint(), entry);
      setString("localPort", conn->localEndpoint(), entry);
    } else {
      setBool("connected", false, entry);
      setString("endpoint", "N/A", entry);
      setString("localPort", "N/A", entry);
    }
    setString("username", config.username, entry);
    setString("password", config.password, entry);
    setString("jwtToken", config.builder.jwtToken(), entry);

    table->Set(context, TRI_V8_STRING(isolate, id), entry).FromMaybe(false);
  };

  for (auto const& [id, cached] : _handles.inactive()) {
    addEntry(id, cached.connection, cached.config, /*isActive=*/false);
  }

  if (_handles.connection() != nullptr) {
    addEntry(_handles.activeHandle(), _handles.connection(), _handles.config(),
             /*isActive=*/true);
  }

  TRI_V8_RETURN(table);
}

void V8ClientConnection::connectHandle(
    v8::Isolate* isolate, v8::FunctionCallbackInfo<v8::Value> const& args,
    std::string const& handle) {
#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  LOG_TOPIC("8aaaa", TRACE, arangodb::Logger::HTTPCLIENT)
      << "Connecting to handle: " << handle;
#endif
  std::lock_guard<std::recursive_mutex> guard(_lock);
  if (_handles.activeHandle() == handle) {
    // its the currently active one
    TRI_V8_RETURN_TRUE();
    return;
  }
  // check if we have a connection for that endpoint in our cache
  auto cached = _handles.take(handle);
  if (!cached.has_value()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_SIMPLE_CLIENT_COULD_NOT_CONNECT,
                                   "Handle not found in the connection list");
  }
  // cache hit. make it the active connection, caching the previous one. it
  // brings its own renewal state, so an expired token is renewed on the next
  // request
  _handles.activate(handle, std::move(*cached));
  adoptClientSettings(_handles.config());
  // the db object caches what it read via the previous handle, which may
  // have been another database, server or user. it is not refilled here, as
  // switching handles must not send requests
  TRI_ExecuteJavaScriptString(isolate, "require('internal').db._forgetCache();",
                              "forget db object cache", false);

  TRI_V8_RETURN_TRUE();
}

void V8ClientConnection::disconnectHandle(
    v8::Isolate* isolate, v8::FunctionCallbackInfo<v8::Value> const& args,
    std::string const& handle) {
  std::lock_guard<std::recursive_mutex> guard(_lock);
  // check if we have a connection for that endpoint in our cache
  if (_handles.take(handle).has_value()) {
    // cache hit. it is now removed from the cache
    TRI_V8_RETURN_TRUE();
  } else if (!_handles.activeHandle().empty() &&
             _handles.activeHandle() == handle) {
    // our main connection is the one to trash.
    _handles.dropActive();
    TRI_V8_RETURN_TRUE();
  } else {
    // we don't know that connection?
    TRI_V8_RETURN_FALSE();
  }
}

void V8ClientConnection::flushConnectionCache(
    v8::Isolate* isolate, v8::FunctionCallbackInfo<v8::Value> const& args,
    std::string const& handle) {
  std::lock_guard<std::recursive_mutex> guard(_lock);
  _handles.clear();
  TRI_V8_RETURN_TRUE();
}

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
void V8ClientConnection::reconnectWithNewPassword(std::string const& password) {
  _client.setPassword(password);
  this->reconnect();
}
#endif

v8::Local<v8::Value> V8ClientConnection::getData(
    v8::Isolate* isolate, std::string_view location,
    std::unordered_map<std::string, std::string> const& headerFields,
    bool raw) {
  if (raw) {
    return requestDataRaw(isolate, fu::RestVerb::Get, location,
                          v8::Undefined(isolate), headerFields);
  }
  return requestData(isolate, fu::RestVerb::Get, location,
                     v8::Undefined(isolate), headerFields);
}

v8::Local<v8::Value> V8ClientConnection::headData(
    v8::Isolate* isolate, std::string_view location,
    std::unordered_map<std::string, std::string> const& headerFields,
    bool raw) {
  if (raw) {
    return requestDataRaw(isolate, fu::RestVerb::Head, location,
                          v8::Undefined(isolate), headerFields);
  }
  return requestData(isolate, fu::RestVerb::Head, location,
                     v8::Undefined(isolate), headerFields);
}

v8::Local<v8::Value> V8ClientConnection::deleteData(
    v8::Isolate* isolate, std::string_view location,
    v8::Local<v8::Value> const& body,
    std::unordered_map<std::string, std::string> const& headerFields,
    bool raw) {
  if (raw) {
    return requestDataRaw(isolate, fu::RestVerb::Delete, location, body,
                          headerFields);
  }
  return requestData(isolate, fu::RestVerb::Delete, location, body,
                     headerFields);
}

v8::Local<v8::Value> V8ClientConnection::optionsData(
    v8::Isolate* isolate, std::string_view location,
    v8::Local<v8::Value> const& body,
    std::unordered_map<std::string, std::string> const& headerFields,
    bool raw) {
  if (raw) {
    return requestDataRaw(isolate, fu::RestVerb::Options, location, body,
                          headerFields);
  }
  return requestData(isolate, fu::RestVerb::Options, location, body,
                     headerFields);
}

v8::Local<v8::Value> V8ClientConnection::postData(
    v8::Isolate* isolate, std::string_view location,
    v8::Local<v8::Value> const& body,
    std::unordered_map<std::string, std::string> const& headerFields, bool raw,
    bool isFile) {
  if (raw) {
    return requestDataRaw(isolate, fu::RestVerb::Post, location, body,
                          headerFields);
  }
  return requestData(isolate, fu::RestVerb::Post, location, body, headerFields,
                     isFile);
}

v8::Local<v8::Value> V8ClientConnection::putData(
    v8::Isolate* isolate, std::string_view location,
    v8::Local<v8::Value> const& body,
    std::unordered_map<std::string, std::string> const& headerFields,
    bool raw) {
  if (raw) {
    return requestDataRaw(isolate, fu::RestVerb::Put, location, body,
                          headerFields);
  }
  return requestData(isolate, fu::RestVerb::Put, location, body, headerFields);
}

v8::Local<v8::Value> V8ClientConnection::patchData(
    v8::Isolate* isolate, std::string_view location,
    v8::Local<v8::Value> const& body,
    std::unordered_map<std::string, std::string> const& headerFields,
    bool raw) {
  if (raw) {
    return requestDataRaw(isolate, fu::RestVerb::Patch, location, body,
                          headerFields);
  }
  return requestData(isolate, fu::RestVerb::Patch, location, body,
                     headerFields);
}

int fuerteToArangoErrorCode(fu::Error ec) {
  ErrorCode errorNumber = TRI_ERROR_NO_ERROR;
  switch (ec) {
    case fu::Error::CouldNotConnect:
    case fu::Error::CloseRequested:
    case fu::Error::ConnectionClosed:
      errorNumber = TRI_ERROR_SIMPLE_CLIENT_COULD_NOT_CONNECT;
      break;

    case fu::Error::ReadError:
      errorNumber = TRI_ERROR_SIMPLE_CLIENT_COULD_NOT_READ;
      break;

    case fu::Error::WriteError:
      errorNumber = TRI_ERROR_SIMPLE_CLIENT_COULD_NOT_WRITE;
      break;

    case fu::Error::RequestTimeout:
      errorNumber = TRI_ERROR_HTTP_REQUEST_TIMEOUT;
      break;

    case fu::Error::QueueCapacityExceeded:
    case fu::Error::ConnectionCanceled:
    case fu::Error::ProtocolError:
    default:
      errorNumber = TRI_ERROR_SIMPLE_CLIENT_UNKNOWN_ERROR;
      break;
  }
  return static_cast<int>(errorNumber);
}

// V8 -> fuerte
void translateHeaders(
    fu::Request& request, fu::RestVerb const method, std::string_view location,
    std::string const& databaseName, bool forceJson,
    std::chrono::duration<double> const& requestTimeout,
    std::unordered_map<std::string, std::string> const& headerFields,
    bool requestCompression) {
  request.header.restVerb = method;
  request.header.database = databaseName;
  request.header.parseArangoPath(location);
  if (forceJson) {
    // Preset posting json (if) but allow override if there is a specified
    // header:
    request.header.contentType(fu::ContentType::Json);
    request.header.acceptType(fu::ContentType::Json);
  }
  for (auto const& pair : headerFields) {
    request.header.addMeta(basics::StringUtils::tolower(pair.first),
                           pair.second);
  }
  if (request.header.acceptType() == fu::ContentType::Unset) {
    request.header.acceptType(fu::ContentType::VPack);
  }
  if (requestCompression &&
      !request.header.meta().contains(StaticStrings::AcceptEncoding)) {
    request.header.addMeta(StaticStrings::AcceptEncoding,
                           StaticStrings::EncodingDeflate);
  }

  request.timeout(correctTimeoutToExecutionDeadline(
      std::chrono::duration_cast<std::chrono::milliseconds>(requestTimeout)));
}

// V8 -> fuerte
bool setRequestBody(fu::Request& request, v8::Isolate* isolate,
                    v8::Local<v8::Value> const& body,
                    velocypack::Options const& vpackOptions, bool forceJson,
                    bool isFile, uint64_t compressRequestThreshold) {
  auto compressIfEligible = [&](uint8_t const* body, size_t size) {
    if (isFile) {
      // we don't compress file bodies
      return false;
    }
    if (compressRequestThreshold == 0) {
      // opted out of compression
      return false;
    }
    if (request.header.meta().contains(StaticStrings::ContentEncoding)) {
      // we don't compress if there is already a Content-Encoding header
      return false;
    }
    if (size < compressRequestThreshold) {
      // body too small for compression
      return false;
    }
    auto& pfm = request.payloadForModification();
    TRI_ASSERT(pfm.empty());
    if (encoding::zlibDeflate(body, size, pfm) != TRI_ERROR_NO_ERROR) {
      TRI_V8_SET_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "unable to compress request body");
      return false;
    }
    // compression successful
    if (pfm.size() >= size) {
      // size after compression is higher than before compression...
      // clear again and fall back to regular, uncompressed encoding.
      pfm.clear();
      return false;
    }

    // add "content-encoding: deflate" header
    TRI_ASSERT(compressRequestThreshold > 0);
    TRI_ASSERT(!request.header.meta().contains(StaticStrings::ContentEncoding));

    request.header.addMeta(StaticStrings::ContentEncoding,
                           StaticStrings::EncodingDeflate);
    return true;
  };

  if (isFile) {
    std::string const inFile = TRI_ObjectToString(isolate, body);
    std::error_code existsEc;
    if (!std::filesystem::exists(inFile, existsEc)) {
      std::string err =
          absl::StrCat("file to load for body doesn't exist: ", inFile);
      TRI_V8_SET_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, err);
      return false;
    }
    std::string contents;
    try {
      contents = FileUtils::slurp(inFile);
    } catch (...) {
      std::string err = absl::StrCat("could not read file", inFile);
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_errno(), err);
    }
    request.header.contentType(fu::ContentType::Custom);
    request.addBinary(reinterpret_cast<uint8_t const*>(contents.data()),
                      contents.length());
  } else if (body->IsString() || body->IsStringObject()) {  // assume JSON
    TRI_Utf8ValueNFC bodyString(isolate, body);
    if (!compressIfEligible(reinterpret_cast<uint8_t const*>(*bodyString),
                            bodyString.length())) {
      // add JSON body without compression
      request.addBinary(reinterpret_cast<uint8_t const*>(*bodyString),
                        bodyString.length());
    }
    if (request.header.contentType() == fu::ContentType::Unset) {
      request.header.contentType(fu::ContentType::Json);
    }
  } else if (body->IsObject() && V8Buffer::hasInstance(isolate, body)) {
    // supplied body is a Buffer object
    char const* data = V8Buffer::data(isolate, body.As<v8::Object>());
    size_t size = V8Buffer::length(isolate, body.As<v8::Object>());

    if (data == nullptr) {
      TRI_V8_SET_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "invalid <body> buffer value");
      return false;
    }
    if (!compressIfEligible(reinterpret_cast<uint8_t const*>(data), size)) {
      // add body from buffer without compression
      request.addBinary(reinterpret_cast<uint8_t const*>(data), size);
    }
  } else if (!body->IsNullOrUndefined()) {
    VPackBuffer<uint8_t> buffer;
    VPackBuilder builder(buffer, &vpackOptions);
    TRI_V8ToVPack(isolate, builder, body, false);
    if (forceJson) {
      auto resultJson = builder.slice().toJson();
      char const* resStr = resultJson.c_str();
      if (!compressIfEligible(reinterpret_cast<uint8_t const*>(resStr),
                              resultJson.size())) {
        // add body without compression
        request.addBinary(reinterpret_cast<uint8_t const*>(resStr),
                          resultJson.size());
      }
      request.header.contentType(fu::ContentType::Json);
    } else {
      if (!compressIfEligible(buffer.data(), buffer.size())) {
        // add body without compression
        request.addVPack(std::move(buffer));
      }
      request.header.contentType(fu::ContentType::VPack);
    }
  } else {
    // body is null or undefined
    if (request.header.contentType() == fu::ContentType::Unset) {
      request.header.contentType(fu::ContentType::Json);
    }
  }

  return true;
}

bool canParseResponse(fu::Response const& response) {
  return (response.isContentTypeVPack() || response.isContentTypeJSON()) &&
         (response.contentEncoding() == fuerte::ContentEncoding::Identity ||
          response.contentEncoding() == fuerte::ContentEncoding::Gzip ||
          response.contentEncoding() == fuerte::ContentEncoding::Deflate) &&
         response.payloadSize() > 0;
}

ErrorCode uncompressResponse(fu::Response& response) {
  ErrorCode code = TRI_ERROR_NO_ERROR;
  if (response.contentEncoding() == fuerte::ContentEncoding::Deflate ||
      response.contentEncoding() == fuerte::ContentEncoding::Gzip) {
    // transparently handling deflate/gzip encoded responses
    auto responseBody = response.payload();
    VPackBuffer<uint8_t> inflateBuf;
    if (response.contentEncoding() == fuerte::ContentEncoding::Deflate) {
      code = arangodb::encoding::zlibInflate(
          reinterpret_cast<uint8_t const*>(responseBody.data()),
          responseBody.size(), inflateBuf);
    } else {
      TRI_ASSERT(response.contentEncoding() == fuerte::ContentEncoding::Gzip);
      code = arangodb::encoding::gzipUncompress(
          reinterpret_cast<uint8_t const*>(responseBody.data()),
          responseBody.size(), inflateBuf);
    }
    if (code != TRI_ERROR_NO_ERROR) {
      return code;
    }
    // replace response body with uncompressed value
    response.setPayload(std::move(inflateBuf), 0);
    // reset Content-Encoding, so that we do not accidentially
    // uncompress again somewhere downstream
    response.header.contentEncoding(fu::ContentEncoding::Identity);
    response.header.removeMeta(StaticStrings::ContentEncoding);
  }
  return code;
}

v8::Local<v8::Value> parseReplyBodyToV8(fu::Response& response,
                                        v8::Isolate* isolate) {
  if (response.contentType() != fu::ContentType::VPack &&
      response.contentType() != fu::ContentType::Json) {
    return v8::Undefined(isolate);
  }

  if (auto code = uncompressResponse(response); code != TRI_ERROR_NO_ERROR) {
    std::string err("Error inflating compressed response body");
    TRI_CreateErrorObject(isolate, code, err, true);
    return v8::Undefined(isolate);
  }

  if (response.contentType() == fu::ContentType::VPack) {
    auto const& slices = response.slices();
    return TRI_VPackToV8(isolate, slices[0]);
  }
  TRI_ASSERT(response.contentType() == fu::ContentType::Json);
  auto responseBody = response.payload();
  try {
    auto parsedBody = VPackParser::fromJson(
        reinterpret_cast<char const*>(responseBody.data()),
        responseBody.size());
    return TRI_VPackToV8(isolate, parsedBody->slice());
  } catch (std::exception const& ex) {
    std::string err =
        absl::StrCat("Error parsing the server JSON reply: ", ex.what());
    TRI_CreateErrorObject(isolate, TRI_ERROR_HTTP_CORRUPTED_JSON, err, true);
  }
  return v8::Undefined(isolate);
}

v8::Local<v8::Value> translateResultBodyToV8(fu::Response& response,
                                             v8::Isolate* isolate) {
  if (auto code = uncompressResponse(response); code != TRI_ERROR_NO_ERROR) {
    std::string err("Error inflating compressed response body");
    TRI_CreateErrorObject(isolate, code, err, true);
    return v8::Undefined(isolate);
  }

  auto responseBody = response.payload();
  if (response.contentEncoding() == fuerte::ContentEncoding::Identity &&
      (response.isContentTypeJSON() || response.isContentTypeText() ||
       response.isContentTypeHtml())) {
    char const* bodyStr = reinterpret_cast<char const*>(responseBody.data());
    return TRI_V8_PAIR_STRING(isolate, bodyStr, responseBody.size());
  }
  V8Buffer* buffer =
      V8Buffer::New(isolate, static_cast<char const*>(responseBody.data()),
                    responseBody.size());
  return v8::Local<v8::Object>::New(isolate, buffer->_handle);
}

void setResultMessage(v8::Isolate* isolate, v8::Local<v8::Context> context,
                      bool isError, unsigned lastHttpReturnCode,
                      std::string const& message,
                      v8::Local<v8::Object> result) {
  result
      ->Set(context, TRI_V8_STD_STRING(isolate, StaticStrings::Error),
            v8::Boolean::New(isolate, true))
      .FromMaybe(isError);
  result
      ->Set(context, TRI_V8_STD_STRING(isolate, StaticStrings::ErrorNum),
            v8::Integer::New(isolate, lastHttpReturnCode))
      .FromMaybe(false);
  result
      ->Set(context, TRI_V8_STD_STRING(isolate, StaticStrings::ErrorMessage),
            TRI_V8_STD_STRING(isolate, message))
      .FromMaybe(false);
}

void setResultMessage(v8::Isolate* isolate, v8::Local<v8::Context> context,
                      bool isError, unsigned lastHttpReturnCode,
                      v8::Local<v8::Object> result) {
  // create raw response
  result
      ->Set(context, TRI_V8_ASCII_STD_STRING(isolate, StaticStrings::Code),
            v8::Integer::New(isolate, lastHttpReturnCode))
      .FromMaybe(false);

  if (lastHttpReturnCode >= 400) {
    std::string msg(GeneralResponse::responseString(
        static_cast<ResponseCode>(lastHttpReturnCode)));
    setResultMessage(isolate, context, isError, lastHttpReturnCode, msg,
                     result);
  } else {
    result
        ->Set(context, TRI_V8_STD_STRING(isolate, StaticStrings::Error),
              v8::Boolean::New(isolate, false))
        .FromMaybe(false);
  }
}

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

v8::Local<v8::Value> V8ClientConnection::requestData(
    v8::Isolate* isolate, fu::RestVerb method, std::string_view location,
    v8::Local<v8::Value> const& body,
    std::unordered_map<std::string, std::string> const& headerFields,
    bool isFile) {
  bool retry = true;

again:
  auto req = std::make_unique<fu::Request>();
  translateHeaders(*req, method, location, databaseName(), _forceJson,
                   _requestTimeout, headerFields, _client.compressTransfer());

  if (!setRequestBody(*req, isolate, body, _vpackOptions, _forceJson, isFile,
                      _client.compressTransfer()
                          ? _client.compressRequestThreshold()
                          : 0)) {
    return v8::Undefined(isolate);
  }

  std::shared_ptr<fu::Connection> connection = acquireConnection(false);
  if (!connection || connection->state() == fu::Connection::State::Closed) {
    TRI_V8_SET_EXCEPTION_MESSAGE(TRI_ERROR_SIMPLE_CLIENT_COULD_NOT_CONNECT,
                                 "not connected");
    return v8::Undefined(isolate);
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
    goto again;
  }

  auto context = TRI_IGETC;
  // not complete
  if (!response) {
    v8::Local<v8::Object> result = v8::Object::New(isolate);
    auto errorNumber = fuerteToArangoErrorCode(rc);
    _lastErrorMessage = fu::to_string(rc);
    _lastHttpReturnCode = static_cast<int>(rest::ResponseCode::SERVER_ERROR);

    setResultMessage(isolate, context, true, errorNumber, _lastErrorMessage,
                     result);
    result
        ->Set(context, TRI_V8_ASCII_STD_STRING(isolate, StaticStrings::Code),
              v8::Integer::New(isolate, _lastHttpReturnCode))
        .FromMaybe(false);

    return result;
  }

  TRI_ASSERT(response != nullptr);

  // complete
  _lastHttpReturnCode = response->statusCode();

  // got a body
  if (canParseResponse(*response)) {
    return parseReplyBodyToV8(*response, isolate);
  }

  auto payloadSize = response->payloadSize();
  if (payloadSize > 0) {
    return translateResultBodyToV8(*response, isolate);
  }
  // no body
  v8::Local<v8::Object> result = v8::Object::New(isolate);
  setResultMessage(isolate, context, false, _lastHttpReturnCode, result);
  return result;
}

v8::Local<v8::Value> V8ClientConnection::requestDataRaw(
    v8::Isolate* isolate, fu::RestVerb method, std::string_view location,
    v8::Local<v8::Value> const& body,
    std::unordered_map<std::string, std::string> const& headerFields) {
  bool retry = true;

again:
  auto req = std::make_unique<fu::Request>();
  translateHeaders(*req, method, location, databaseName(), _forceJson,
                   _requestTimeout, headerFields, _client.compressTransfer());

  if (!setRequestBody(*req, isolate, body, _vpackOptions, _forceJson,
                      /*isFile*/ false,
                      _client.compressTransfer()
                          ? _client.compressRequestThreshold()
                          : 0)) {
    return v8::Undefined(isolate);
  }

  std::shared_ptr<fu::Connection> connection = acquireConnection(false);
  if (!connection || connection->state() == fu::Connection::State::Closed) {
    TRI_V8_SET_EXCEPTION_MESSAGE(TRI_ERROR_SIMPLE_CLIENT_COULD_NOT_CONNECT,
                                 "not connected");
    return v8::Undefined(isolate);
  }
  fu::Error rc = fu::Error::NoError;
  std::unique_ptr<fu::Response> response;
  try {
    response = connection->sendRequest(std::move(req));
  } catch (fu::Error const& e) {
    rc = e;
    _lastErrorMessage.assign(fu::to_string(e));
    _lastHttpReturnCode = 503;
  }

  if (rc == fu::Error::ConnectionClosed && retry) {
    retry = false;
    goto again;
  }

  auto context = TRI_IGETC;
  // not complete
  v8::Local<v8::Object> result = v8::Object::New(isolate);
  if (!response) {
    setResultMessage(isolate, context, true, _lastHttpReturnCode,
                     _lastErrorMessage, result);
    return result;
  }

  // complete
  _lastHttpReturnCode = response->statusCode();
  setResultMessage(isolate, context, false, _lastHttpReturnCode, result);

  v8::Local<v8::Object> headers = v8::Object::New(isolate);
  result->Set(context, TRI_V8_ASCII_STRING(isolate, "headers"), headers)
      .FromMaybe(false);

  if (canParseResponse(*response)) {
    result
        ->Set(context, TRI_V8_STD_STRING(isolate, StaticStrings::ParsedBody),
              parseReplyBodyToV8(*response, isolate))
        .FromMaybe(false);
  }
  auto payloadSize = response->payloadSize();
  if (payloadSize > 0) {
    result
        ->Set(context, TRI_V8_STD_STRING(isolate, StaticStrings::Body),
              translateResultBodyToV8(*response, isolate))
        .FromMaybe(false);
  }

  if (response->contentType() != fuerte::ContentType::Custom) {
    auto contentType =
        TRI_V8_STD_STRING(isolate, fu::to_string(response->contentType()));
    headers
        ->Set(context,
              TRI_V8_STD_STRING(isolate, StaticStrings::ContentTypeHeader),
              contentType)
        .FromMaybe(false);
  }
  for (auto const& it : response->header.meta()) {
    headers
        ->Set(context, TRI_V8_STD_STRING(isolate, it.first),
              TRI_V8_STD_STRING(isolate, it.second))
        .FromMaybe(false);
  }

  return result;
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
