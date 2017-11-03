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

#include "RestAdminServerHandler.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestAdminServerHandler::RestAdminServerHandler(GeneralRequest* request,
                                       GeneralResponse* response)
    : RestBaseHandler(request, response) {}

RestStatus RestAdminServerHandler::execute() {
  std::vector<std::string> const& suffixes = _request->suffixes();
  if (!suffixes.empty() && suffixes[0] == "mode") {
    handleMode();
  } else {
    generateError(rest::ResponseCode::NOT_FOUND,
        TRI_ERROR_HTTP_NOT_FOUND, "not found");
  }

  return RestStatus::DONE;
}

void RestAdminServerHandler::writeResult(ServerState::Mode const& mode) {
    VPackBuilder builder;
    builder.add(
        VPackValue(ServerState::modeToString(mode))
    );
    generateOk(rest::ResponseCode::OK, builder.slice());
}

void RestAdminServerHandler::handleMode() {
    auto const requestType = _request->requestType();
    if (requestType == rest::RequestType::GET) {
        writeResult(ServerState::serverMode());
    } else if (requestType == rest::RequestType::PUT) {
        bool parseSuccess;
        std::shared_ptr<VPackBuilder> parsedBody =
            parseVelocyPackBody(parseSuccess);
        if (!parseSuccess) {
            generateError(rest::ResponseCode::BAD,
                TRI_ERROR_HTTP_BAD_PARAMETER, "invalid JSON");
            return;
        }

        auto slice = parsedBody->slice();
        if (!slice.isString()) {
            generateError(rest::ResponseCode::BAD,
                TRI_ERROR_HTTP_BAD_PARAMETER, "body must be a json encoded string");
            return;
        }

        auto newMode = ServerState::stringToMode(slice.copyString());
        if (newMode == ServerState::Mode::INVALID) {
            generateError(rest::ResponseCode::BAD,
                TRI_ERROR_HTTP_BAD_PARAMETER, "mode invalid");
            return;
        }

        auto currentMode = ServerState::serverMode();
        // very restrictive...only default and readonly user writeable
        if (
            newMode == currentMode
            ||
            (newMode == ServerState::Mode::DEFAULT
            && currentMode == ServerState::Mode::READ_ONLY)
            ||
            (currentMode == ServerState::Mode::DEFAULT
            && newMode == ServerState::Mode::READ_ONLY)
        ) {
            auto res = ServerState::instance()->propagateClusterServerMode(newMode);
            if (!res.ok()) {
                generateError(rest::ResponseCode::BAD,
                    TRI_ERROR_HTTP_SERVER_ERROR, "couldn't set requested mode");
                LOG_TOPIC(ERR, Logger::FIXME) << "Couldn't set requested mode: " <<
                    res.errorMessage();
            } else {
                generateError(rest::ResponseCode::BAD,
                    TRI_ERROR_HTTP_BAD_PARAMETER, "cannot set requested mode");
            }
            writeResult(newMode);
        } else {
            generateError(rest::ResponseCode::BAD,
                TRI_ERROR_HTTP_BAD_PARAMETER, "cannot set requested mode");
        }
    } else {
        generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
            TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    }
}