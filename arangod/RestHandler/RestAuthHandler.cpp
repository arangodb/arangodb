////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Andreas Streichardt
////////////////////////////////////////////////////////////////////////////////

#include "RestAuthHandler.h"

#include <fuerte/jwt.h>
#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/ScopeGuard.h"
#include "Basics/StringUtils.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Utils/Events.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestAuthHandler::RestAuthHandler(
    application_features::ApplicationServer& server, GeneralRequest* request,
    GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response) {}

RestStatus RestAuthHandler::execute() {
  auto const type = _request->requestType();
  if (type != rest::RequestType::POST) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
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
          resultBuilder.add("jwt", VPackValue(generateJwt(_request->user())));
        }
        // otherwise we will send an empty body back. callers must handle
        // this case!
      }

      generateDocument(resultBuilder.slice(), true, &VPackOptions::Defaults);
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

  VPackSlice usernameSlice = slice.get("username");
  VPackSlice passwordSlice = slice.get("password");

  if (!usernameSlice.isString() || !passwordSlice.isString()) {
    return badRequest();
  }

  std::string const username = usernameSlice.copyString();
  std::string const password = passwordSlice.copyString();

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

  if (um->checkPassword(username, password)) {
    VPackBuilder resultBuilder;
    {
      VPackObjectBuilder b(&resultBuilder);
      resultBuilder.add("jwt", VPackValue(generateJwt(username)));
    }

    isValid = true;
    generateDocument(resultBuilder.slice(), true, &VPackOptions::Defaults);
  } else {
    // mop: rfc 2616 10.4.2 (if credentials wrong 401)
    generateError(rest::ResponseCode::UNAUTHORIZED, TRI_ERROR_HTTP_UNAUTHORIZED,
                  "Wrong credentials");
  }
  return RestStatus::DONE;
}

std::string RestAuthHandler::generateJwt(std::string const& username) const {
  AuthenticationFeature* af = AuthenticationFeature::instance();
  TRI_ASSERT(af != nullptr);
  return fuerte::jwt::generateUserToken(
      af->tokenCache().jwtSecret(), username,
      std::chrono::seconds(uint64_t(af->sessionTimeout())));
}

RestStatus RestAuthHandler::badRequest() {
  generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                "invalid JSON");
  return RestStatus::DONE;
}
