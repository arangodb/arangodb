////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "VocbaseContext.h"

#include <velocypack/Builder.h>
#include <velocypack/Exception.h>
#include <velocypack/Parser.h>
#include <velocypack/velocypack-aliases.h>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/tri-strings.h"
#include "Cluster/ServerState.h"
#include "Endpoint/ConnectionInfo.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "Logger/Logger.h"
#include "Ssl/SslInterface.h"
#include "VocBase/AuthInfo.h"
#include "VocBase/vocbase.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

double VocbaseContext::ServerSessionTtl =
    60.0 * 60.0 * 24 * 60;  // 2 month session timeout

VocbaseContext::VocbaseContext(GeneralRequest* request, TRI_vocbase_t* vocbase,
                               std::string const& jwtSecret)
    : RequestContext(request), _vocbase(vocbase), _jwtSecret(jwtSecret) {
  TRI_ASSERT(_vocbase != nullptr);
}

VocbaseContext::~VocbaseContext() { _vocbase->release(); }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not to use special cluster authentication
////////////////////////////////////////////////////////////////////////////////

bool VocbaseContext::useClusterAuthentication() const {
  auto role = ServerState::instance()->getRole();

  if (ServerState::instance()->isDBServer(role)) {
    return true;
  }

  if (ServerState::instance()->isCoordinator(role)) {
    std::string const& s = _request->requestPath();

    if (s == "/_api/shard-comm" || s == "/_admin/shutdown") {
      return true;
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks the authentication
////////////////////////////////////////////////////////////////////////////////

rest::ResponseCode VocbaseContext::authenticate() {
  TRI_ASSERT(_vocbase != nullptr);

  auto restServer = application_features::ApplicationServer::getFeature<GeneralServerFeature>("GeneralServer");

  if (!restServer->authentication()) {
    // no authentication required at all
    return rest::ResponseCode::OK;
  }

  std::string const& path = _request->requestPath();

  // mop: inside authenticateRequest() _request->user will be populated
  bool forceOpen = false;
  rest::ResponseCode result = authenticateRequest(&forceOpen);

  if (result == rest::ResponseCode::UNAUTHORIZED ||
      result == rest::ResponseCode::FORBIDDEN) {
    if (StringUtils::isPrefix(path, "/_open/") ||
        StringUtils::isPrefix(path, "/_admin/aardvark/") || path == "/") {
      // mop: these paths are always callable...they will be able to check
      // req.user when it could be validated
      result = rest::ResponseCode::OK;
      forceOpen = true;
    }
  }

  // check that we are allowed to see the database
  if (result == rest::ResponseCode::OK && !forceOpen) {
    if (!StringUtils::isPrefix(path, "/_api/user/")) {
      std::string const& username = _request->user();
      std::string const& dbname = _request->databaseName();

      if (!username.empty() || !dbname.empty()) {
        AuthLevel level =
            GeneralServerFeature::AUTH_INFO.canUseDatabase(username, dbname);

        if (level != AuthLevel::RW) {
          result = rest::ResponseCode::UNAUTHORIZED;
        }
      }
    }
  }

  return result;
}

rest::ResponseCode VocbaseContext::authenticateRequest(
    bool* forceOpen) {
  
  auto restServer = application_features::ApplicationServer::getFeature<GeneralServerFeature>("GeneralServer");
#ifdef ARANGODB_HAVE_DOMAIN_SOCKETS
  // check if we need to run authentication for this type of
  // endpoint
  ConnectionInfo const& ci = _request->connectionInfo();

  if (ci.endpointType == Endpoint::DomainType::UNIX &&
      !restServer->authenticationUnixSockets()) {
    // no authentication required for unix socket domain connections
    return rest::ResponseCode::OK;
  }
#endif

  std::string const& path = _request->requestPath();

  if (restServer->authenticationSystemOnly()) {
    // authentication required, but only for /_api, /_admin etc.

    if (!path.empty()) {
      // check if path starts with /_
      if (path[0] != '/') {
        *forceOpen = true;
        return rest::ResponseCode::OK;
      }

      if (path.length() > 0 && path[1] != '_') {
        *forceOpen = true;
        return rest::ResponseCode::OK;
      }
    }
  }

  // .............................................................................
  // authentication required
  // .............................................................................

  bool found;
  std::string const& authStr =
      _request->header(StaticStrings::Authorization, found);

  if (!found) {
    return rest::ResponseCode::UNAUTHORIZED;
  }

  size_t methodPos = authStr.find_first_of(' ');
  if (methodPos == std::string::npos) {
    return rest::ResponseCode::UNAUTHORIZED;
  }

  // skip over authentication method
  char const* auth = authStr.c_str() + methodPos;
  while (*auth == ' ') {
    ++auth;
  }

  LOG(DEBUG) << "Authorization header: " << authStr;

  if (TRI_CaseEqualString(authStr.c_str(), "basic ", 6)) {
    return basicAuthentication(auth);
  } else if (TRI_CaseEqualString(authStr.c_str(), "bearer ", 7)) {
    return jwtAuthentication(std::string(auth));
  } else {
    // mop: hmmm is 403 the correct status code? or 401? or 400? :S
    return rest::ResponseCode::UNAUTHORIZED;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks the authentication via basic
////////////////////////////////////////////////////////////////////////////////

rest::ResponseCode VocbaseContext::basicAuthentication(
    const char* auth) {
  if (useClusterAuthentication()) {
    std::string const expected = ServerState::instance()->getAuthentication();

    if (expected.substr(6) != std::string(auth)) {
      return rest::ResponseCode::UNAUTHORIZED;
    }

    std::string const up = StringUtils::decodeBase64(auth);
    std::string::size_type n = up.find(':', 0);

    if (n == std::string::npos || n == 0 || n + 1 > up.size()) {
      LOG(TRACE) << "invalid authentication data found, cannot extract "
                    "username/password";

      return rest::ResponseCode::BAD;
    }

    _request->setUser(up.substr(0, n));

    return rest::ResponseCode::OK;
  }

  AuthResult result = GeneralServerFeature::AUTH_INFO.checkAuthentication(
      AuthInfo::AuthType::BASIC, auth);

  if (!result._authorized) {
    return rest::ResponseCode::UNAUTHORIZED;
  }

  // we have a user name, verify 'mustChange'
  _request->setUser(std::move(result._username));

  if (result._mustChange) {
    if ((_request->requestType() == rest::RequestType::PUT ||
         _request->requestType() == rest::RequestType::PATCH) &&
        StringUtils::isPrefix(_request->requestPath(), "/_api/user/")) {
      return rest::ResponseCode::OK;
    }

    return rest::ResponseCode::FORBIDDEN;
  }

  return rest::ResponseCode::OK;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks the authentication via jwt
////////////////////////////////////////////////////////////////////////////////

rest::ResponseCode VocbaseContext::jwtAuthentication(
    std::string const& auth) {
  AuthResult result = GeneralServerFeature::AUTH_INFO.checkAuthentication(
      AuthInfo::AuthType::JWT, auth);

  if (!result._authorized) {
    return rest::ResponseCode::UNAUTHORIZED;
  }
  // we have a user name, verify 'mustChange'
  _request->setUser(std::move(result._username));
  return rest::ResponseCode::OK;
}
