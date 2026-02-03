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
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////

#include "Abac/AbacAuthorizationClient.h"

#include "ApplicationFeatures/CommunicationFeaturePhase.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Endpoint/Endpoint.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "Ssl/ssl-helper.h"

#include <velocypack/Builder.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>

namespace arangodb::abac {

AbacAuthorizationClient::AbacAuthorizationClient(
    application_features::CommunicationFeaturePhase& comm, Config config)
    : _comm(comm), _config(std::move(config)) {}

AbacAuthorizationClient::~AbacAuthorizationClient() = default;

bool AbacAuthorizationClient::isConfigured() const {
  return !_config.endpoint.empty();
}

Result AbacAuthorizationClient::checkAuthorization(
    std::string const& jsonPayload, std::string const& user,
    std::vector<std::string> const& roles, AuthorizationResponse& response) {
  if (!isConfigured()) {
    // No endpoint configured - fail open or closed based on config
    if (_config.failOpen) {
      response.effect = AuthorizationEffect::kAllow;
      response.message = "ABAC not configured, allowing by default";
      return Result();
    }
    return Result(TRI_ERROR_FORBIDDEN,
                  "ABAC authorization service not configured");
  }

  // Create endpoint
  std::unique_ptr<Endpoint> endpoint(
      Endpoint::clientFactory(_config.endpoint));

  if (endpoint == nullptr) {
    if (_config.failOpen) {
      response.effect = AuthorizationEffect::kAllow;
      response.message = "Invalid endpoint configuration, allowing by default";
      LOG_TOPIC("abac1", WARN, Logger::AUTHORIZATION)
          << "ABAC: Invalid endpoint '" << _config.endpoint << "', failing open";
      return Result();
    }
    return Result(TRI_ERROR_BAD_PARAMETER,
                  "Invalid ABAC authorization endpoint: " + _config.endpoint);
  }

  // Create connection
  constexpr uint64_t sslProtocol = SslProtocol::TLS_V12;
  std::unique_ptr<httpclient::GeneralClientConnection> connection(
      httpclient::GeneralClientConnection::factory(
          _comm, endpoint, _config.requestTimeout, _config.connectTimeout,
          _config.maxRetries, sslProtocol));

  if (connection == nullptr) {
    if (_config.failOpen) {
      response.effect = AuthorizationEffect::kAllow;
      response.message = "Failed to create connection, allowing by default";
      LOG_TOPIC("abac2", WARN, Logger::AUTHORIZATION)
          << "ABAC: Failed to create connection to '" << _config.endpoint
          << "', failing open";
      return Result();
    }
    return Result(TRI_ERROR_INTERNAL,
                  "Failed to create connection to ABAC authorization service");
  }

  // Configure HTTP client
  httpclient::SimpleHttpClientParams params(_config.requestTimeout,
                                            /*warn*/ true);
  params.setKeepAlive(true);
  params.setMaxRetries(_config.maxRetries);

  if (!_config.jwtToken.empty()) {
    params.setJwt(_config.jwtToken);
  }

  httpclient::SimpleHttpClient client(std::move(connection), params);

  // Build the full request body with user and roles
  // The external service expects:
  // {
  //   "user": "username",
  //   "roles": ["role1", "role2"],
  //   "requests": [ ... permission requests from jsonPayload ... ]
  // }
  velocypack::Builder requestBuilder;
  {
    velocypack::ObjectBuilder obj(&requestBuilder);
    requestBuilder.add("user", velocypack::Value(user));

    requestBuilder.add("roles", velocypack::Value(velocypack::ValueType::Array));
    for (auto const& role : roles) {
      requestBuilder.add(velocypack::Value(role));
    }
    requestBuilder.close();  // roles array

    // Parse the jsonPayload and add as "requests"
    auto parsedPayload = velocypack::Parser::fromJson(jsonPayload);
    requestBuilder.add("requests", parsedPayload->slice());
  }

  std::string requestBody = requestBuilder.slice().toJson();

  // Set up headers
  std::unordered_map<std::string, std::string> headers;
  headers["Content-Type"] = "application/json";
  headers["Accept"] = "application/json";

  // Send POST request
  std::unique_ptr<httpclient::SimpleHttpResult> httpResult(client.retryRequest(
      rest::RequestType::POST, _config.apiPath, requestBody.data(),
      requestBody.size(), headers));

  if (httpResult == nullptr) {
    if (_config.failOpen) {
      response.effect = AuthorizationEffect::kAllow;
      response.message =
          "Authorization service unreachable, allowing by default";
      LOG_TOPIC("abac3", WARN, Logger::AUTHORIZATION)
          << "ABAC: Authorization service unreachable at '"
          << _config.endpoint << "', failing open";
      return Result();
    }
    return Result(TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE,
                  "ABAC authorization service unreachable: " +
                      client.getErrorMessage());
  }

  // Check HTTP status
  int httpCode = httpResult->getHttpReturnCode();
  if (httpCode < 200 || httpCode >= 300) {
    std::string errorMsg = "ABAC authorization service returned HTTP " +
                           std::to_string(httpCode);
    if (_config.failOpen) {
      response.effect = AuthorizationEffect::kAllow;
      response.message = errorMsg + ", allowing by default";
      LOG_TOPIC("abac4", WARN, Logger::AUTHORIZATION)
          << "ABAC: " << errorMsg << ", failing open";
      return Result();
    }
    return Result(TRI_ERROR_HTTP_SERVICE_UNAVAILABLE, errorMsg);
  }

  // Parse response
  arangodb::basics::StringBuffer const& body = httpResult->getBody();
  std::string responseBody(body.data(), body.length());

  return parseResponse(responseBody, response);
}

Result AbacAuthorizationClient::parseResponse(std::string const& responseBody,
                                              AuthorizationResponse& response) {
  try {
    auto parsedResponse = velocypack::Parser::fromJson(responseBody);
    velocypack::Slice slice = parsedResponse->slice();

    if (!slice.isObject()) {
      return Result(TRI_ERROR_HTTP_CORRUPTED_JSON,
                    "ABAC response is not a JSON object");
    }

    // Parse effect field
    velocypack::Slice effectSlice = slice.get("effect");
    if (effectSlice.isString()) {
      std::string effectStr = effectSlice.copyString();
      if (effectStr == "ALLOW" || effectStr == "allow") {
        response.effect = AuthorizationEffect::kAllow;
      } else if (effectStr == "DENY" || effectStr == "deny") {
        response.effect = AuthorizationEffect::kDeny;
      } else {
        response.effect = AuthorizationEffect::kUnknown;
      }
    } else {
      response.effect = AuthorizationEffect::kUnknown;
    }

    // Parse message field (optional)
    velocypack::Slice messageSlice = slice.get("message");
    if (messageSlice.isString()) {
      response.message = messageSlice.copyString();
    }

    return Result();
  } catch (velocypack::Exception const& e) {
    return Result(TRI_ERROR_HTTP_CORRUPTED_JSON,
                  std::string("Failed to parse ABAC response: ") + e.what());
  }
}

}  // namespace arangodb::abac
