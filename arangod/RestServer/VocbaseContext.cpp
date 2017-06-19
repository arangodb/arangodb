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
#include "Basics/Exceptions.h"
#include "Basics/tri-strings.h"
#include "Cluster/ServerState.h"
#include "Endpoint/ConnectionInfo.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Logger/Logger.h"
#include "Rest/GeneralResponse.h"
#include "RestServer/FeatureCacheFeature.h"
#include "Ssl/SslInterface.h"
#include "Utils/Events.h"
#include "VocBase/AuthInfo.h"
#include "VocBase/vocbase.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

double VocbaseContext::ServerSessionTtl =
    60.0 * 60.0 * 24 * 60;  // 2 month session timeout

VocbaseContext::VocbaseContext(GeneralRequest* request, TRI_vocbase_t* vocbase)
    : RequestContext(request),
    _vocbase(vocbase),
    _authentication(nullptr) {
  TRI_ASSERT(_vocbase != nullptr);
  _authentication = FeatureCacheFeature::instance()->authenticationFeature();
  TRI_ASSERT(_authentication != nullptr);

  // _vocbase has already been refcounted for us
  TRI_ASSERT(!_vocbase->isDangling());
}

VocbaseContext::~VocbaseContext() { 
  TRI_ASSERT(!_vocbase->isDangling());
  _vocbase->release(); 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks the authentication
////////////////////////////////////////////////////////////////////////////////

rest::ResponseCode VocbaseContext::authenticate() {
  TRI_ASSERT(_vocbase != nullptr);
  
  if (!_authentication->isActive()) {
    // no authentication required at all
    return rest::ResponseCode::OK;
  }

  std::string const& path = _request->requestPath();

  // mop: inside authenticateRequest() _request->user will be populated
  bool forceOpen = false;
  rest::ResponseCode result = authenticateRequest();

  if (result == rest::ResponseCode::OK && !_request->user().empty()) {
    auto authContext = _authentication->authInfo()->getAuthContext(
        _request->user(), _request->databaseName());
    _request->setExecContext(
      new ExecContext(_request->user(), _request->databaseName(), authContext));
  }

  if (result == rest::ResponseCode::UNAUTHORIZED ||
      result == rest::ResponseCode::FORBIDDEN) {

#ifdef ARANGODB_HAVE_DOMAIN_SOCKETS
    // check if we need to run authentication for this type of
    // endpoint
    ConnectionInfo const& ci = _request->connectionInfo();

    if (ci.endpointType == Endpoint::DomainType::UNIX &&
        !_authentication->authenticationUnixSockets()) {
      // no authentication required for unix domain socket connections
      forceOpen = true;
      result = rest::ResponseCode::OK;
    }
#endif

    if (result != rest::ResponseCode::OK &&
        _authentication->authenticationSystemOnly()) {
      // authentication required, but only for /_api, /_admin etc.

      if (!path.empty()) {
        // check if path starts with /_
        if (path[0] != '/') {
          forceOpen = true;
          result = rest::ResponseCode::OK;
        }

        // path begins with /
        if (path.size() > 1 && path[1] != '_') {
          forceOpen = true;
          result = rest::ResponseCode::OK;
        }
      }
    }

    if (result != rest::ResponseCode::OK) {
      if (StringUtils::isPrefix(path, "/_open/") ||
          StringUtils::isPrefix(path, "/_admin/aardvark/") || path == "/") {
        // mop: these paths are always callable...they will be able to check
        // req.user when it could be validated
        result = rest::ResponseCode::OK;
        forceOpen = true;
      }
    }
  }
  
  
  if (result != rest::ResponseCode::OK) {
    return result;
  }
  
  std::string const& username = _request->user();
  // mop: internal request => no username present
  if (username.empty()) {
    // mop: set user to root so that the foxx stuff
    // knows about us
    return rest::ResponseCode::OK;
  }

  // check that we are allowed to see the database
  if (!forceOpen) {
    // check for GET /_db/_system/_api/user/USERNAME/database
    std::string pathWithUser = std::string("/_api/user/") + username;

    if (_request->requestType() == RequestType::GET &&
      (StringUtils::isPrefix(path, pathWithUser) || (StringUtils::isPrefix(path, "/_admin/aardvark/"))) ) {
      _request->setExecContext(nullptr);
      return rest::ResponseCode::OK;
    }

    if (!StringUtils::isPrefix(path, "/_api/user/")) {
      std::string const& dbname = _request->databaseName();
      if (!username.empty() || !dbname.empty()) {
        AuthLevel level =
            _authentication->canUseDatabase(username, dbname);

        if (level == AuthLevel::NONE) {
          events::NotAuthorized(_request);
          result = rest::ResponseCode::UNAUTHORIZED;
        }
      }
    }
  }

  return result;
}

rest::ResponseCode VocbaseContext::authenticateRequest() {
  bool found;
  std::string const& authStr =
      _request->header(StaticStrings::Authorization, found);

  if (!found) {
    events::CredentialsMissing(_request);
    return rest::ResponseCode::UNAUTHORIZED;
  }

  size_t methodPos = authStr.find_first_of(' ');

  if (methodPos != std::string::npos) {
    // skip over authentication method
    char const* auth = authStr.c_str() + methodPos;

    while (*auth == ' ') {
      ++auth;
    }

    LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "Authorization header: " << authStr;

    try {
      // note that these methods may throw in case of an error
      if (TRI_CaseEqualString(authStr.c_str(), "basic ", 6)) {
        return basicAuthentication(auth);
      }
      if (TRI_CaseEqualString(authStr.c_str(), "bearer ", 7)) {
        return jwtAuthentication(std::string(auth));
      }
      // fallthrough intentional
    } catch (arangodb::basics::Exception const& ex) {
      // translate error
      if (ex.code() == TRI_ERROR_USER_NOT_FOUND) {
        return rest::ResponseCode::UNAUTHORIZED;
      }
      return GeneralResponse::responseCode(ex.what());
    } catch (...) {
      return rest::ResponseCode::SERVER_ERROR;
    }
  }

  events::UnknownAuthenticationMethod(_request);
  return rest::ResponseCode::UNAUTHORIZED;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks the authentication via basic
////////////////////////////////////////////////////////////////////////////////

rest::ResponseCode VocbaseContext::basicAuthentication(char const* auth) {
  AuthResult result = _authentication->authInfo()->checkAuthentication(
      AuthInfo::AuthType::BASIC, auth);

  _request->setAuthorized(result._authorized);
  if (!result._authorized) {
    events::CredentialsBad(_request, rest::AuthenticationMethod::BASIC);
    return rest::ResponseCode::UNAUTHORIZED;
  }

  _request->setUser(std::move(result._username));

  // we have a user name, verify 'mustChange'
  if (result._mustChange) {
    if ((_request->requestType() == rest::RequestType::PUT ||
         _request->requestType() == rest::RequestType::PATCH) &&
        StringUtils::isPrefix(_request->requestPath(), "/_api/user/")) {
      return rest::ResponseCode::OK;
    }

    events::PasswordChangeRequired(_request);
    return rest::ResponseCode::FORBIDDEN;
  }

  events::Authenticated(_request, rest::AuthenticationMethod::BASIC);
  return rest::ResponseCode::OK;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks the authentication via jwt
////////////////////////////////////////////////////////////////////////////////

rest::ResponseCode VocbaseContext::jwtAuthentication(std::string const& auth) {
  AuthResult result = _authentication->authInfo()->checkAuthentication(
      AuthInfo::AuthType::JWT, auth);
  
  _request->setAuthorized(result._authorized);
  if (!result._authorized) {
    events::CredentialsBad(_request, rest::AuthenticationMethod::JWT);
    return rest::ResponseCode::UNAUTHORIZED;
  }
  
  _request->setUser(std::move(result._username));
  events::Authenticated(_request, rest::AuthenticationMethod::JWT);

  return rest::ResponseCode::OK;
}
