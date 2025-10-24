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
/// @author Andreas Streichardt
////////////////////////////////////////////////////////////////////////////////

#include "RestAuthHandler.h"

#include "Auth/Handler.h"
#include "Auth/TokenCache.h"
#include "Auth/UserManager.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StringUtils.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Utils/Events.h"

#include <fuerte/jwt.h>
#include <velocypack/Builder.h>

#include <format>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestAuthHandler::RestAuthHandler(ArangodServer& server, GeneralRequest* request,
                                 GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response) {}

RestStatus RestAuthHandler::execute() {
  auto const type = _request->requestType();
  if (type != rest::RequestType::POST) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    return RestStatus::DONE;
  }

  if (!AuthenticationFeature::instance()->isActive()) {
    // Since 3.12.6 we actually mount this RestHandler in the case that
    // server authentication is switched off. This is because we need
    // that an `arangosh` can run our tests even in this case by just
    // using username and password. Since from 3.12.6 on we use `/_open/auth`
    // in this case in the client tools, we need to get some positive
    // response, if only with an `invalid` token. If server authentication
    // is switched off, then even that invalid token will be accepted.
    VPackBuilder builder;
    {
      VPackObjectBuilder guard(&builder);
      builder.add("jwt", VPackValue("invalid"));
    }
    generateResult(rest::ResponseCode::OK, builder.slice());
    return RestStatus::DONE;
  }
  auth::UserManager* um = AuthenticationFeature::instance()->userManager();
  if (um == nullptr) {
    std::string msg = "This server does not support users";
    LOG_TOPIC("2e7d4", WARN, Logger::AUTHENTICATION) << msg;
    generateError(rest::ResponseCode::UNAUTHORIZED, TRI_ERROR_HTTP_UNAUTHORIZED,
                  msg);
    return RestStatus::DONE;
  }

  auto const& suffixes = _request->suffixes();

  if (suffixes.size() == 1 && suffixes[0] == "renew") {
    // JWT token renew request
    if (!_request->authenticated() || _request->user().empty() ||
        _request->authenticationMethod() !=
            arangodb::rest::AuthenticationMethod::JWT) {
      generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_USER_NOT_FOUND);
    } else {
      VPackBuilder resultBuilder;
      {
        VPackObjectBuilder b(&resultBuilder);
        // only return a new token if the current token is about to expire
        if (_request->tokenExpiry() > 0.0 &&
            _request->tokenExpiry() - TRI_microtime() < 150.0) {
          AuthenticationFeature* af = AuthenticationFeature::instance();
          std::chrono::seconds expiry(
              static_cast<uint64_t>(af->sessionTimeout()));
          resultBuilder.add("jwt",
                            VPackValue(generateJwt(_request->user(), expiry)));
        }
        // otherwise we will send an empty body back. callers must handle
        // this case!
      }

      generateResult(rest::ResponseCode::OK, resultBuilder.slice());
    }
    return RestStatus::DONE;
  }

  bool parseSuccess = false;
  VPackSlice slice = this->parseVPackBody(parseSuccess);
  if (!parseSuccess) {  // error already set
    return RestStatus::DONE;
  }

  if (!slice.isObject()) {
    return badRequest();
  }

  AuthenticationFeature* af = AuthenticationFeature::instance();
  VPackSlice usernameSlice = slice.get("username");
  VPackSlice passwordSlice = slice.get("password");
  VPackSlice expiryTimeSlice = slice.get("expiryTime");

  std::string username;
  std::string password;
  double expiryTimeSeconds = af->sessionTimeout();  // default value

  if (usernameSlice.isString() && passwordSlice.isString()) {
    username = usernameSlice.copyString();
    password = passwordSlice.copyString();
  } else if (passwordSlice.isString()) {
    password = passwordSlice.copyString();
  } else {
    return badRequest();
  }

  // Handle optional expiryTime parameter
  if (expiryTimeSlice.isNumber()) {
    expiryTimeSeconds = expiryTimeSlice.getNumber<double>();

    // Validate against min/max bounds
    if (expiryTimeSeconds < af->minimalJwtExpiryTime()) {
      generateError(
          rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
          std::format(
              "expiryTime is below the minimal allowed value of {} seconds",
              af->minimalJwtExpiryTime()));
      return RestStatus::DONE;
    }

    if (expiryTimeSeconds > af->maximalJwtExpiryTime()) {
      generateError(
          rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
          std::format(
              "expiryTime exceeds the maximal allowed value of {} seconds",
              af->maximalJwtExpiryTime()));
      return RestStatus::DONE;
    }
  } else if (!expiryTimeSlice.isNone()) {
    // expiryTime was provided but is not a number
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expiryTime must be a number");
    return RestStatus::DONE;
  }

  bool isValid = false;

  auto guard = scopeGuard([&]() noexcept {
    try {
      if (isValid) {
        events::LoggedIn(*_request, username);
      } else {
        events::CredentialsBad(*_request, username);
      }
    } catch (...) {
      // nothing we can do
    }
  });

  std::string un;
  if (um->checkCredentials(username, password, un)) {
    VPackBuilder resultBuilder;
    {
      VPackObjectBuilder b(&resultBuilder);
      std::chrono::seconds expiry(static_cast<uint64_t>(expiryTimeSeconds));
      resultBuilder.add("jwt", VPackValue(generateJwt(un, expiry)));
    }

    isValid = true;
    generateResult(rest::ResponseCode::OK, resultBuilder.slice());
  } else {
    // mop: rfc 2616 10.4.2 (if credentials wrong 401)
    generateError(rest::ResponseCode::UNAUTHORIZED, TRI_ERROR_HTTP_UNAUTHORIZED,
                  "Wrong credentials");
  }
  return RestStatus::DONE;
}

std::string RestAuthHandler::generateJwt(
    std::string const& username, std::chrono::seconds expiryTime) const {
  AuthenticationFeature* af = AuthenticationFeature::instance();
  TRI_ASSERT(af != nullptr);
  return fuerte::jwt::generateUserToken(af->tokenCache().jwtSecret(), username,
                                        expiryTime);
}

RestStatus RestAuthHandler::badRequest() {
  generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                "invalid JSON");
  return RestStatus::DONE;
}
