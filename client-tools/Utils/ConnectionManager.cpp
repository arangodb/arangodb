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

#include "Utils/ConnectionManager.h"

#include "Assertions/Assert.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Logger/LogMacros.h"
#include "Rest/Version.h"
#include "Ssl/SslInterface.h"
#include "Ssl/jwt.h"

#include <absl/strings/str_cat.h>
#include <boost/hash2/md5.hpp>
#include <fuerte/requests.h>
#include <velocypack/Builder.h>
#include <velocypack/Parser.h>

namespace fu = arangodb::fuerte;

namespace arangodb {
namespace {
// return an identifier to a connection configuration, consisting of
// endpoint, username, password, jwt, authentication and protocol type.
// the time claims of the jwt are left out, so that a renewed or regenerated
// token for the same identity keeps the identifier
std::string connectionIdentifier(fu::ConnectionBuilder const& builder) {
  std::string const jwtToken = builder.jwtToken();
  std::string const raw = absl::StrCat(
      builder.normalizedEndpoint(), "/", builder.user(), "/",
      builder.password(), "/",
      rest::SslInterface::jwt::extractTimelessClaims(jwtToken).value_or(
          jwtToken),
      "/", to_string(builder.authenticationType()), "/",
      to_string(builder.protocolType()));
  boost::hash2::md5_128 hash;
  hash.update(raw.c_str(), raw.size());
  std::string hexString{boost::hash2::to_string(hash.result())};

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  LOG_TOPIC("9aaaa", TRACE, Logger::HTTPCLIENT)
      << "Connection identifier " << hexString << " calculated from: " << raw;
#endif
  return hexString;
}

}  // namespace

ConnectionManager::ConnectionManager(fu::ConnectionBuilder base,
                                     Connect connect, TimeSource now)
    : _connect(std::move(connect)), _now(std::move(now)) {
  _handles.config().builder = std::move(base);
}

auto ConnectionManager::configure(ConnectionSettings const& settings,
                                  std::chrono::milliseconds requestTimeout)
    -> Result {
  // configure a copy, so that a failed login leaves the configuration
  // untouched. each authentication mode clears the fields of the others;
  // leftovers would become part of the connection identifier
  fu::ConnectionBuilder builder = _handles.config().builder;
  builder.endpoint(settings.endpoint);
  TokenRenewal renewal;
  auto const useJwt = [&builder](std::string const& token) {
    builder.jwtToken(token).user("").password("").authenticationType(
        fu::AuthenticationType::Jwt);
  };

  // check jwtToken first, then jwtSecret, as they are empty by default,
  // but username defaults to "root" in most configurations
  TRI_ASSERT(settings.jwtToken.empty() || settings.jwtSecret.empty());

  if (!settings.jwtToken.empty()) {
    useJwt(settings.jwtToken);
    renewal.kind = TokenRenewal::Kind::kJwtTokenOption;
  } else if (!settings.jwtSecret.empty()) {
    useJwt(internalToken(settings.jwtSecret));
  } else if (!settings.username.empty()) {
    auto const login =
        requestToken(TokenRequest::kLogin, builder, requestTimeout,
                     settings.username, settings.password);
    if (login.fail()) {
      return login.result();
    }
    if (auto const& token = login.get(); token.has_value()) {
      useJwt(*token);
      renewal = TokenRenewal{.kind = TokenRenewal::Kind::kLogin,
                             .token = tokenStateFor(*token, _now())};
    } else {
      // the server does not implement the _open/auth API, or has
      // authentication disabled and issues no real token: use basic auth
      builder.user(settings.username)
          .password(settings.password)
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
                                    .settings = settings};
  return {};
}

auto ConnectionManager::connect(bool bypassCache, bool forceJson)
    -> ConnectOutcome {
  auto const& endpoint = _handles.config().settings.endpoint;
  auto const fail = [&](unsigned httpCode, std::string message,
                        fu::Connection& connection) {
    LOG_TOPIC("9daab", DEBUG, Logger::HTTPCLIENT)
        << "Connection attempt to endpoint '" << endpoint
        << "' failed: " << message << " from: " << connection.localEndpoint();
    return ConnectOutcome{.httpCode = httpCode,
                          .errorMessage = std::move(message)};
  };

  // the cache has one connection per handle. the active connection is only
  // replaced once the new one has proven to work
  std::string const id = connectionIdentifier(_handles.config().builder);
  ConnectionHandles<Configuration>::Entry candidate{nullptr, _handles.config()};
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
    candidate.connection = _connect(candidate.config.builder);
  }
  auto newConnection = candidate.connection;
  // a kept connection may have been closed by the server meanwhile
  int retryCount = wasFromCache ? 2 : 1;
  fu::StringMap params{{"details", "true"}};
  while (retryCount > 0) {
    auto req = fu::createRequest(fu::RestVerb::Get, "/_api/version", params);
    if (forceJson) {
      req->header.acceptType(fu::ContentType::Json);
    }
    req->header.database = candidate.config.settings.databaseName;
    req->timeout(std::chrono::seconds(30));
    retryCount -= 1;
    try {
      auto res = newConnection->sendRequest(std::move(req));
      if (!res) {
        return fail(500, "unable to create connection", *newConnection);
      }

      unsigned const httpCode = res->statusCode();
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
      if (httpCode >= 400) {
        auto const& headers = res->messageHeader().meta();
        if (auto it = headers.find("http/1.1"); it != headers.end()) {
          std::string errorMessage = it->second;
          if (body.isObject()) {
            std::string const msg = basics::VelocyPackHelper::getStringValue(
                body, StaticStrings::ErrorMessage, "");
            if (!msg.empty()) {
              errorMessage = msg;
            }
          }
          return fail(httpCode, std::move(errorMessage), *newConnection);
        }
      }

      if (!body.isObject()) {
        return fail(503,
                    absl::StrCat("invalid response: '",
                                 std::string_view(reinterpret_cast<char const*>(
                                                      res->payload().data()),
                                                  res->payload().size()),
                                 "'"),
                    *newConnection);
      }

      // from here on, newConnection is the active connection
      _handles.activate(id, std::move(candidate));
      ConnectOutcome outcome{.connection = newConnection, .httpCode = httpCode};

      if (basics::VelocyPackHelper::getStringValue(body, "server", "") !=
          "arango") {
        return outcome;
      }
      auto& server = outcome.server.emplace();
      server.version =
          basics::VelocyPackHelper::getStringValue(body, "version", "");
      if (VPackSlice const details = body.get("details"); details.isObject()) {
        if (VPackSlice const mode = details.get("mode"); mode.isString()) {
          server.mode = mode.copyString();
        }
        if (VPackSlice const role = details.get("role"); role.isString()) {
          server.role = role.copyString();
        }
      }
      // without a version number, the server probably runs in hardened mode
      if (body.hasKey("version") &&
          rest::Version::parseVersionString(server.version).first < 3) {
        newConnection->cancel();
        outcome.httpCode = 500;
        outcome.errorMessage =
            absl::StrCat("Server version number ('", server.version,
                         "') is too low. Expecting 3.0 or higher");
      }
      return outcome;
    } catch (fu::Error const& e) {
      if (retryCount <= 0) {
        return fail(503, fu::to_string(e), *newConnection);
      }
      newConnection->cancel();
      newConnection = _connect(candidate.config.builder);
      candidate.connection = newConnection;
    }
  }
  return ConnectOutcome{};
}

auto ConnectionManager::requestToken(TokenRequest request,
                                     fu::ConnectionBuilder const& target,
                                     std::chrono::milliseconds timeout,
                                     std::string const& username,
                                     std::string const& password)
    -> TokenOutcome {
  try {
    std::shared_ptr<fu::Connection> connection;
    if (request == TokenRequest::kRenew && _handles.connection() != nullptr &&
        _handles.connection()->state() == fu::Connection::State::Connected &&
        _handles.config().builder.jwtToken() == target.jwtToken()) {
      // the active connection already authenticates with the token
      connection = _handles.connection();
    } else {
      // a temporary connection to the endpoint of target and with its
      // settings, which does not report failures as errors of the client's
      // connection. a login sends credentials instead of a token
      fu::ConnectionBuilder tempBuilder = target;
      tempBuilder.onFailure(nullptr);
      if (request == TokenRequest::kLogin) {
        tempBuilder.user("").password("").jwtToken("").authenticationType(
            fu::AuthenticationType::None);
      }
      connection = _connect(tempBuilder);
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
    req->timeout(timeout);

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
    return TokenOutcome::error(TRI_ERROR_FAILED, fu::to_string(ec));
  } catch (...) {
    // intentional fall through to error
  }
  return TokenOutcome::error(TRI_ERROR_FAILED, "unknown error");
}

auto ConnectionManager::renewTokenIfDue(
    JwtClock::duration threshold, std::string const& jwtTokenOption,
    std::chrono::milliseconds requestTimeout) -> bool {
  auto& config = _handles.config();
  std::string newToken;
  switch (config.renewal.kind) {
    case TokenRenewal::Kind::kNone:
      return false;
    case TokenRenewal::Kind::kJwtTokenOption:
      // the client renews the jwt token option in the background, at the
      // endpoint of the active handle
      newToken = jwtTokenOption;
      break;
    case TokenRenewal::Kind::kLogin: {
      auto const now = _now();
      if (!isRenewalDue(config.renewal.token, now, threshold)) {
        return false;
      }
      // both at the server of this connection, as the user it was created for
      auto const outcome = renewOrLogin(
          config.renewal.token, now,
          [&] {
            return requestToken(TokenRequest::kRenew, config.builder,
                                requestTimeout);
          },
          [&] {
            return requestToken(TokenRequest::kLogin, config.builder,
                                requestTimeout, config.settings.username,
                                config.settings.password);
          });
      if (outcome.fail()) {
        LOG_TOPIC("7552f", WARN, Logger::AUTHENTICATION)
            << "renewing the JWT token of user '" << config.settings.username
            << "' at '" << config.settings.endpoint
            << "' failed: " << outcome.errorMessage() << "; retrying in "
            << kRenewalRetryInterval.count() << " seconds at the earliest";
      }
      config.renewal.token =
          applyRenewal(std::move(config.renewal.token), outcome, _now());
      newToken = config.renewal.token.token;
      break;
    }
  }
  if (newToken.empty() || newToken == config.builder.jwtToken()) {
    return false;
  }
  // its identifier stays the same, so a connection with it supersedes the
  // active one
  config.builder.jwtToken(newToken);
  return true;
}

auto ConnectionManager::internalToken(std::string const& jwtSecret)
    -> std::string const& {
  // superuser tokens do not expire, and a new one would only differ in its
  // "iat" claim. reusing it lets a reconnect reuse the cached connection,
  // which keeps the token it was created with
  if (_internalToken.empty() || _internalTokenSecret != jwtSecret) {
    _internalToken =
        rest::SslInterface::jwt::generateInternalToken(jwtSecret, "arangosh");
    _internalTokenSecret = jwtSecret;
  }
  return _internalToken;
}

auto ConnectionManager::activeConnection() const
    -> std::shared_ptr<fu::Connection> const& {
  return _handles.connection();
}

auto ConnectionManager::activeHandle() const -> std::string const& {
  return _handles.activeHandle();
}

auto ConnectionManager::settings() const -> ConnectionSettings const& {
  return _handles.config().settings;
}

void ConnectionManager::setDatabaseName(std::string const& databaseName) {
  _handles.config().settings.databaseName = databaseName;
}

auto ConnectionManager::protocolType() const -> fu::ProtocolType {
  return _handles.config().builder.protocolType();
}

auto ConnectionManager::handleTable() const -> std::vector<HandleInfo> {
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
                               .username = config.settings.username,
                               .password = config.settings.password,
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

auto ConnectionManager::switchTo(std::string const& handle) -> HandleSwitch {
#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  LOG_TOPIC("8aaaa", TRACE, Logger::HTTPCLIENT)
      << "Connecting to handle: " << handle;
#endif
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
  return HandleSwitch::kSwitched;
}

auto ConnectionManager::drop(std::string const& handle) -> bool {
  if (_handles.take(handle).has_value()) {
    return true;
  }
  if (!_handles.activeHandle().empty() && _handles.activeHandle() == handle) {
    _handles.dropActive();
    return true;
  }
  return false;
}

void ConnectionManager::clear() { _handles.clear(); }

}  // namespace arangodb
