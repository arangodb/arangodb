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
#include "GeneralServer/AuthenticationFeature.h"
#include "Logger/Logger.h"
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
}

VocbaseContext::~VocbaseContext() { _vocbase->release(); }

////////////////////////////////////////////////////////////////////////////////
/// @brief checks the authentication
////////////////////////////////////////////////////////////////////////////////

rest::ResponseCode VocbaseContext::authenticate() {
  TRI_ASSERT(_vocbase != nullptr);

  if (!_authentication->isEnabled()) {
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
  
  if (result != rest::ResponseCode::OK) {
    return result;
  }
  
  std::string const& username = _request->user();
  // mop: internal request => no username present
  if (username.empty()) {
    return rest::ResponseCode::OK;
  }

  // check that we are allowed to see the database
  if (!forceOpen) {
    if (!StringUtils::isPrefix(path, "/_api/user/")) {
      std::string const& dbname = _request->databaseName();
      
      if (!username.empty() || !dbname.empty()) {
        AuthLevel level =
            _authentication->canUseDatabase(username, dbname);

        if (level != AuthLevel::RW) {
          events::NotAuthorized(_request);
          result = rest::ResponseCode::UNAUTHORIZED;
        }
      }
    }
  }

  return result;
}

rest::ResponseCode VocbaseContext::authenticateRequest(bool* forceOpen) {
#ifdef ARANGODB_HAVE_DOMAIN_SOCKETS
  // check if we need to run authentication for this type of
  // endpoint
  ConnectionInfo const& ci = _request->connectionInfo();

  if (ci.endpointType == Endpoint::DomainType::UNIX &&
      !_authentication->authenticationUnixSockets()) {
    // no authentication required for unix socket domain connections
    return rest::ResponseCode::OK;
  }
#endif

  std::string const& path = _request->requestPath();

  if (_authentication->authenticationSystemOnly()) {
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
    events::CredentialsMissing(_request);
    return rest::ResponseCode::UNAUTHORIZED;
  }

  size_t methodPos = authStr.find_first_of(' ');

  if (methodPos == std::string::npos) {
    events::UnknownAuthenticationMethod(_request);
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
    events::UnknownAuthenticationMethod(_request);
    return rest::ResponseCode::UNAUTHORIZED;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks the authentication via basic
////////////////////////////////////////////////////////////////////////////////

rest::ResponseCode VocbaseContext::basicAuthentication(char const* auth) {
  AuthResult result = _authentication->authInfo()->checkAuthentication(
      AuthInfo::AuthType::BASIC, auth);

  if (!result._authorized) {
    events::CredentialsBad(_request, rest::AuthenticationMethod::BASIC);
    return rest::ResponseCode::UNAUTHORIZED;
  }

  if (!result._username.empty()) {
    _request->setUser(std::move(result._username));
  } else {
    _request->setUser("root");
  }

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

  if (!result._authorized) {
    events::CredentialsBad(_request, rest::AuthenticationMethod::JWT);
    return rest::ResponseCode::UNAUTHORIZED;
  }

  // we have a user name, verify 'mustChange'
  if (!result._username.empty()) {
    _request->setUser(std::move(result._username));
  } else {
    _request->setUser("root");
  }
  events::Authenticated(_request, rest::AuthenticationMethod::JWT);

  return rest::ResponseCode::OK;
}
