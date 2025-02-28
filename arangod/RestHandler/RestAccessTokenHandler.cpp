////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2025 ArangoDB GmbH, Cologne, Germany
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
/// @author Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "RestAccessTokenHandler.h"

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

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestAccessTokenHandler::RestAccessTokenHandler(ArangodServer& server,
                                               GeneralRequest* request,
                                               GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response) {}

RestStatus RestAccessTokenHandler::execute() {
  auth::UserManager* um = AuthenticationFeature::instance()->userManager();
  if (um == nullptr) {
    std::string msg = "This server does not support users";
    LOG_TOPIC("2e7d5", WARN, Logger::AUTHENTICATION) << msg;
    generateError(rest::ResponseCode::UNAUTHORIZED, TRI_ERROR_HTTP_UNAUTHORIZED,
                  msg);
    return RestStatus::DONE;
  }

  auto const& suffixes = _request->suffixes();

  if (suffixes.size() < 1) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "path parameter 'user' is missing");
    return RestStatus::DONE;
  }

  std::string const& user = suffixes[0];

  if (!canAccessUser(user)) {
    generateError(ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN);
    return RestStatus::DONE;
  }

  auto const type = _request->requestType();

  switch (type) {
    case RequestType::GET:
      return showAccessTokens(um, user);
    case RequestType::POST:
      return createAccessToken(um, user);
    case RequestType::DELETE_REQ:
      return deleteAccessToken(um, user);

    default:
      generateError(ResponseCode::BAD, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
      return RestStatus::DONE;
  }
}

RestStatus RestAccessTokenHandler::showAccessTokens(auth::UserManager* um,
                                                    std::string const& user) {
  VPackBuilder tokens;
  Result result = um->accessTokens(user, tokens);

  if (result.ok()) {
    generateResult(ResponseCode::OK, tokens.slice());
  } else {
    generateError(result);
  }

  return RestStatus::DONE;
}

RestStatus RestAccessTokenHandler::createAccessToken(auth::UserManager* um,
                                                     std::string const& user) {
  bool parseSuccess = false;
  VPackSlice const body = this->parseVPackBody(parseSuccess);
  if (!parseSuccess || !body.isObject()) {
    generateError(ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER);
    return RestStatus::DONE;
  }

  VPackSlice s = body.get("name");
  std::string name = s.isString() ? s.copyString() : "";

  VPackSlice v = body.get("valid_until");
  double validUntil = basics::VelocyPackHelper::getNumericValue<double>(v, 0);

  VPackBuilder token;
  Result result = um->createAccessToken(user, name, validUntil, token);

  if (result.ok()) {
    generateResult(ResponseCode::OK, token.slice());
  } else {
    generateError(result);
  }

  return RestStatus::DONE;
}

RestStatus RestAccessTokenHandler::deleteAccessToken(auth::UserManager* um,
                                                     std::string const& user) {
  auto const& suffixes = _request->suffixes();

  if (suffixes.size() != 2) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "path parameter 'id' is missing");
    return RestStatus::DONE;
  }

  uint64_t id = StringUtils::uint64(suffixes[1]);
  Result result = um->deleteAccessToken(user, id);

  if (result.ok()) {
    resetResponse(ResponseCode::OK);
  } else {
    generateError(result);
  }

  return RestStatus::DONE;
}
