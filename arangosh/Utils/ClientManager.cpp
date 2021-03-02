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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#include "ClientManager.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/application-exit.h"
#include "Logger/Logger.h"
#include "Rest/Version.h"
#include "Shell/ClientFeature.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"

#include <fuerte/jwt.h>

namespace {

arangodb::Result getHttpErrorMessage(arangodb::httpclient::SimpleHttpResult* result) {
  using arangodb::basics::VelocyPackHelper;
  using arangodb::basics::StringUtils::itoa;

  if (!result) {
    // no result to pull from
    return {TRI_ERROR_INTERNAL, "no response from server!"};
  }

  auto code = TRI_ERROR_NO_ERROR;
  // set base message
  std::string message("got error from server: HTTP " + itoa(result->getHttpReturnCode()) +
                      " (" + result->getHttpReturnMessage() + ")");

  // assume vpack body, catch otherwise
  try {
    std::shared_ptr<VPackBuilder> parsedBody = result->getBodyVelocyPack();
    VPackSlice const body = parsedBody->slice();

    auto serverCode = VelocyPackHelper::getNumericValue<int>(body, "errorNum", 0);
    std::string const& serverMessage =
        VelocyPackHelper::getStringValue(body, "errorMessage", "");

    if (serverCode > 0) {
      code = ErrorCode{serverCode};
      message.append(": ArangoError " + itoa(serverCode) + ": " + serverMessage);
    }
  } catch (...) {
    // no need to recover, fallthrough for default error message
  }
  return {code, std::move(message)};
}

}  // namespace

namespace arangodb {

ClientManager::ClientManager(application_features::ApplicationServer& server, LogTopic& topic)
    : _server(server), _topic{topic} {}

ClientManager::~ClientManager() = default;

Result ClientManager::getConnectedClient(std::unique_ptr<httpclient::SimpleHttpClient>& httpClient,
                                         bool force, bool logServerVersion,
                                         bool logDatabaseNotFound, bool quiet) {
  TRI_ASSERT(_server.hasFeature<HttpEndpointProvider>());
  ClientFeature& client = _server.getFeature<HttpEndpointProvider, ClientFeature>();

  try {
    httpClient = client.createHttpClient();
  } catch (...) {
    LOG_TOPIC("2b5fd", FATAL, _topic) << "cannot create server connection, giving up!";
    return {TRI_ERROR_SIMPLE_CLIENT_COULD_NOT_CONNECT};
  }

  // set client parameters
  std::string dbName = client.databaseName();
  httpClient->params().setLocationRewriter(static_cast<void*>(&client), &rewriteLocation);
  httpClient->params().setUserNamePassword("/", client.username(), client.password());
  if (!client.jwtSecret().empty()) {
    httpClient->params().setJwt(fuerte::jwt::generateInternalToken(client.jwtSecret(), client.endpoint()));
  }

  // now connect by retrieving version
  ErrorCode errorCode = TRI_ERROR_NO_ERROR;
  std::string const versionString = httpClient->getServerVersion(&errorCode);
  if (TRI_ERROR_NO_ERROR != errorCode) {
    if (!quiet && (TRI_ERROR_ARANGO_DATABASE_NOT_FOUND != errorCode || logDatabaseNotFound)) {
      // arangorestore does not log "database not found" errors in case
      // it tries to create the database...
      LOG_TOPIC("775bd", ERR, _topic)
          << "Could not connect to endpoint '" << client.endpoint() << "', database: '"
          << dbName << "', username: '" << client.username() << "'";
      LOG_TOPIC("b1ad6", ERR, _topic) << "Error message: '" << httpClient->getErrorMessage() << "'";
    }
    return {errorCode};
  }

  if (versionString.empty() || versionString == "arango") {
    // server running in hardened mode?
    return {TRI_ERROR_NO_ERROR};
  }

  if (!quiet && logServerVersion) {
    // successfully connected
    LOG_TOPIC("06792", INFO, _topic) << "Server version: " << versionString;
  }

  // validate server version
  std::pair<int, int> version = rest::Version::parseVersionString(versionString);
  if (version.first < 3) {
    // we can connect to 3.x
    if (!quiet) {
      LOG_TOPIC("c4add", ERR, _topic)
          << "Error: got incompatible server version '" << versionString << "'";
    }

    if (!force) {
      return {TRI_ERROR_INCOMPATIBLE_VERSION};
    }
  }

  return {TRI_ERROR_NO_ERROR};
}

std::unique_ptr<httpclient::SimpleHttpClient> ClientManager::getConnectedClient(
    bool force, bool logServerVersion, bool logDatabaseNotFound) {
  std::unique_ptr<httpclient::SimpleHttpClient> httpClient;

  Result result = getConnectedClient(httpClient, force, logServerVersion,
                                     logDatabaseNotFound, false);
  if (result.fail() && !(force && result.is(TRI_ERROR_INCOMPATIBLE_VERSION))) {
    FATAL_ERROR_EXIT();
  }

  return httpClient;
}

std::string ClientManager::rewriteLocation(void* data, std::string const& location) {
  // if it already starts with "/_db/", we are done
  if (location.compare(0, 5, "/_db/") == 0) {
    return location;
  }

  // assume data is `ClientFeature*`; must be enforced externally
  ClientFeature* client = static_cast<ClientFeature*>(data);
  std::string const& dbname = client->databaseName();

  // prefix with `/_db/${dbname}/`
  if (location[0] == '/') {
    // already have leading "/", leave it off
    return "/_db/" + dbname + location;
  }
  return "/_db/" + dbname + "/" + location;
}

std::pair<Result, std::string> ClientManager::getArangoIsCluster(httpclient::SimpleHttpClient& client) {
  using arangodb::basics::VelocyPackHelper;

  Result result{TRI_ERROR_NO_ERROR};
  std::unique_ptr<httpclient::SimpleHttpResult> response(
      client.request(rest::RequestType::GET, "/_admin/server/role", "", 0));

  if (response == nullptr || !response->isComplete()) {
    result.reset(TRI_ERROR_INTERNAL, "no response from server!");
    return {result, ""};
  }

  std::string role = "UNDEFINED";

  if (response->getHttpReturnCode() == (int)rest::ResponseCode::OK) {
    try {
      std::shared_ptr<VPackBuilder> parsedBody = response->getBodyVelocyPack();
      VPackSlice const body = parsedBody->slice();
      role = VelocyPackHelper::getStringValue(body, "role", "UNDEFINED");
    } catch (...) {
      // No Action
    }
  } else {
    if (response->wasHttpError()) {
      result = ::getHttpErrorMessage(response.get());
      LOG_TOPIC("0d964", ERR, _topic)
          << "got error while checking cluster mode: " << result.errorMessage();
      client.setErrorMessage(result.errorMessage(), false);
    } else {
      result.reset(TRI_ERROR_INTERNAL);
    }

    client.disconnect();
  }

  return {result, role};
}

std::pair<Result, bool> ClientManager::getArangoIsUsingEngine(httpclient::SimpleHttpClient& client,
                                                              std::string const& name) {
  using arangodb::basics::VelocyPackHelper;

  Result result{TRI_ERROR_NO_ERROR};
  std::unique_ptr<httpclient::SimpleHttpResult> response(
      client.request(rest::RequestType::GET, "/_api/engine", "", 0));
  if (response == nullptr || !response->isComplete()) {
    result.reset(TRI_ERROR_INTERNAL, "no response from server!");
    return {result, false};
  }

  std::string engine = "UNDEFINED";

  if (response->getHttpReturnCode() == static_cast<int>(rest::ResponseCode::OK)) {
    try {
      std::shared_ptr<VPackBuilder> parsedBody = response->getBodyVelocyPack();
      VPackSlice const body = parsedBody->slice();
      engine = VelocyPackHelper::getStringValue(body, "name", "UNDEFINED");
    } catch (...) {
      // No Action
    }
  } else {
    if (response->wasHttpError()) {
      result = ::getHttpErrorMessage(response.get());
      LOG_TOPIC("b05c4", ERR, _topic) << "got error while checking storage engine: "
                             << result.errorMessage();
      client.setErrorMessage(result.errorMessage(), false);
    } else {
      result.reset(TRI_ERROR_INTERNAL);
    }

    client.disconnect();
  }

  return {result, (engine == name)};
}

}  // namespace arangodb
