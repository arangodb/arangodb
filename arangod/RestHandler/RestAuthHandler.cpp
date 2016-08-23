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
/// @author Andreas Streichardt
////////////////////////////////////////////////////////////////////////////////

#include "RestAuthHandler.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/StringUtils.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "Logger/Logger.h"
#include "Rest/HttpRequest.h"
#include "Ssl/SslInterface.h"
#include "VocBase/AuthInfo.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestAuthHandler::RestAuthHandler(GeneralRequest* request,
                                 GeneralResponse* response,
                                 std::string const* jwtSecret)
    : RestVocbaseBaseHandler(request, response),
      _jwtSecret(*jwtSecret),
      _validFor(60 * 60 * 24 * 30) {}

bool RestAuthHandler::isDirect() const { return false; }

std::string RestAuthHandler::generateJwt(std::string const& username,
                                         std::string const& password) {
  VPackBuilder headerBuilder;
  {
    VPackObjectBuilder h(&headerBuilder);
    headerBuilder.add("alg", VPackValue("HS256"));
    headerBuilder.add("typ", VPackValue("JWT"));
  }

  std::chrono::seconds exp =
      std::chrono::duration_cast<std::chrono::seconds>(
          std::chrono::system_clock::now().time_since_epoch()) +
      _validFor;
  VPackBuilder bodyBuilder;
  {
    VPackObjectBuilder p(&bodyBuilder);
    bodyBuilder.add("preferred_username", VPackValue(username));
    bodyBuilder.add("iss", VPackValue("arangodb"));
    bodyBuilder.add("exp", VPackValue(exp.count()));
  }

  std::string fullMessage(StringUtils::encodeBase64(headerBuilder.toJson()) +
                          "." +
                          StringUtils::encodeBase64(bodyBuilder.toJson()));
  std::string signature =
      sslHMAC(_jwtSecret.c_str(), _jwtSecret.length(), fullMessage.c_str(),
              fullMessage.length(), SslInterface::Algorithm::ALGORITHM_SHA256);

  return fullMessage + "." + StringUtils::encodeBase64U(signature);
}

RestHandler::status RestAuthHandler::execute() {
  auto const type = _request->requestType();
  if (type != rest::RequestType::POST) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    return status::DONE;
  }

  VPackOptions options = VPackOptions::Defaults;
  options.checkAttributeUniqueness = true;

  bool parseSuccess;
  std::shared_ptr<VPackBuilder> parsedBody =
      parseVelocyPackBody(&options, parseSuccess);
  if (!parseSuccess) {
    return badRequest();
  }

  VPackSlice slice = parsedBody->slice();
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

  AuthResult auth =
      GeneralServerFeature::AUTH_INFO.checkPassword(username, password);

  if (auth._authorized) {
    VPackBuilder resultBuilder;
    {
      VPackObjectBuilder b(&resultBuilder);
      std::string jwt = generateJwt(username, password);
      resultBuilder.add("jwt", VPackValue(jwt));
      resultBuilder.add("must_change_password", VPackValue(auth._mustChange));
    }

    generateDocument(resultBuilder.slice(), true, &VPackOptions::Defaults);
    return status::DONE;
  } else {
    // mop: rfc 2616 10.4.2 (if credentials wrong 401)
    generateError(rest::ResponseCode::UNAUTHORIZED,
                  TRI_ERROR_HTTP_UNAUTHORIZED, "Wrong credentials");
    return status::DONE;
  }
}

RestHandler::status RestAuthHandler::badRequest() {
  generateError(rest::ResponseCode::BAD,
                TRI_ERROR_HTTP_BAD_PARAMETER, "invalid JSON");
  return status::DONE;
}
