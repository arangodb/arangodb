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

#include "Basics/MutexLocker.h"
#include "Basics/tri-strings.h"
#include "Cluster/ServerState.h"
#include "Endpoint/ConnectionInfo.h"
#include "Logger/Logger.h"
#include "RestServer/RestServerFeature.h"
#include "VocBase/AuthInfo.h"
#include "VocBase/server.h"
#include "VocBase/vocbase.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

double VocbaseContext::ServerSessionTtl =
    60.0 * 60.0 * 24 * 60;  // 2 month session timeout

VocbaseContext::VocbaseContext(HttpRequest* request, TRI_vocbase_t* vocbase)
    : RequestContext(request), _vocbase(vocbase) {}

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

GeneralResponse::ResponseCode VocbaseContext::authenticate() {
  TRI_ASSERT(_vocbase != nullptr);

  if (!_vocbase->_settings.requireAuthentication) {
    // no authentication required at all
    return GeneralResponse::ResponseCode::OK;
  }

#ifdef ARANGODB_HAVE_DOMAIN_SOCKETS
  // check if we need to run authentication for this type of
  // endpoint
  ConnectionInfo const& ci = _request->connectionInfo();

  if (ci.endpointType == Endpoint::DomainType::UNIX &&
      !_vocbase->_settings.requireAuthenticationUnixSockets) {
    // no authentication required for unix socket domain connections
    return GeneralResponse::ResponseCode::OK;
  }
#endif

  std::string const& path = _request->requestPath();

  if (_vocbase->_settings.authenticateSystemOnly) {
    // authentication required, but only for /_api, /_admin etc.

    if (!path.empty()) {
      // check if path starts with /_
      if (path[0] != '/') {
        return GeneralResponse::ResponseCode::OK;
      }

      if (path[0] != '\0' && path[1] != '_') {
        return GeneralResponse::ResponseCode::OK;
      }
    }
  }

  if (StringUtils::isPrefix(path, "/_open/") ||
      StringUtils::isPrefix(path, "/_admin/aardvark/") || path == "/") {
    return GeneralResponse::ResponseCode::OK;
  }

  // .............................................................................
  // authentication required
  // .............................................................................

  bool found;
  std::string const& auth =
      _request->header(StaticStrings::Authorization, found);

  if (!found) {
    return GeneralResponse::ResponseCode::UNAUTHORIZED;
  }

  if (useClusterAuthentication()) {
    std::string const expected = ServerState::instance()->getAuthentication();

    if (expected.substr(6) != std::string(auth)) {
      return GeneralResponse::ResponseCode::UNAUTHORIZED;
    }

    // TODO should support other authentications (currently only "basic ")
    std::string const up = StringUtils::decodeBase64(auth.substr(6));
    std::string::size_type n = up.find(':', 0);

    if (n == std::string::npos || n == 0 || n + 1 > up.size()) {
      LOG(TRACE) << "invalid authentication data found, cannot extract "
                    "username/password";

      return GeneralResponse::ResponseCode::BAD;
    }

    _request->setUser(up.substr(0, n));

    return GeneralResponse::ResponseCode::OK;
  }

  AuthResult result =
      RestServerFeature::AUTH_INFO.checkAuthentication(auth, _vocbase->_name);

  if (!result._authorized) {
    return GeneralResponse::ResponseCode::UNAUTHORIZED;
  }

  // we have a user name, verify 'mustChange'
  _request->setUser(std::move(result._username));

  if (result._mustChange) {
    if ((_request->requestType() == GeneralRequest::RequestType::PUT ||
         _request->requestType() == GeneralRequest::RequestType::PATCH) &&
        StringUtils::isPrefix(_request->requestPath(), "/_api/user/")) {
      return GeneralResponse::ResponseCode::OK;
    }

    return GeneralResponse::ResponseCode::FORBIDDEN;
  }

  return GeneralResponse::ResponseCode::OK;
}
