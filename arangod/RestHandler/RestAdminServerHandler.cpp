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

#include "Actions/RestActionHandler.h"
#include "Replication/ReplicationFeature.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestAdminServerHandler::RestAdminServerHandler(GeneralRequest* request,
                                       GeneralResponse* response)
    : RestBaseHandler(request, response) {}

RestStatus RestAdminServerHandler::execute() {
  std::vector<std::string> const& suffixes = _request->suffixes();
  if (suffixes.size() == 1 && suffixes[0] == "mode") {
    handleMode();
  } else if (suffixes.size() == 1 && suffixes[0] == "id") {
    handleId();
  } else if (suffixes.size() == 1 && suffixes[0] == "role") {
    handleRole();
  } else {
    generateError(rest::ResponseCode::NOT_FOUND, 404);
  }
  return RestStatus::DONE;
}

void RestAdminServerHandler::writeModeResult(ServerState::Mode const& mode) {
  VPackBuilder builder;
  {
    VPackObjectBuilder b(&builder);
    builder.add(
        "mode", VPackValue(ServerState::modeToString(mode))
    );
  }
  generateOk(rest::ResponseCode::OK, builder);
}

void RestAdminServerHandler::handleId() {
  if (_request->requestType() != rest::RequestType::GET) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
      TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    return;
  }

  auto instance = ServerState::instance();
  if (!instance->isRunningInCluster()) {
    // old behaviour...klingt komisch, is aber so
    generateError(rest::ResponseCode::SERVER_ERROR,
      TRI_ERROR_HTTP_SERVER_ERROR);
    return;
  }

  VPackBuilder builder;
  {
    VPackObjectBuilder b(&builder);
    builder.add(
        "id", VPackValue(instance->getId())
    );
  }
  generateOk(rest::ResponseCode::OK, builder);
}

void RestAdminServerHandler::handleRole() {
  if (_request->requestType() != rest::RequestType::GET) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
      TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    return;
  }
  auto state = ServerState::instance();
  bool hasFailover = false;
  auto replicationFeature = ReplicationFeature::INSTANCE;
  if (replicationFeature != nullptr &&
      replicationFeature->isAutomaticFailoverEnabled()) {
    hasFailover = true;
  } 
  VPackBuilder builder;
  {
    VPackObjectBuilder b(&builder);
    builder.add(
       "role", VPackValue(state->roleToString(state->getRole()))
    );
    builder.add(
      "mode", hasFailover ? VPackValue("resilient") : VPackValue("default")
    );
  }
  generateOk(rest::ResponseCode::OK, builder);
}

void RestAdminServerHandler::handleMode() {
    auto const requestType = _request->requestType();
    if (requestType == rest::RequestType::GET) {
        writeModeResult(ServerState::serverMode());
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
        if (!slice.isObject()) {
          generateError(rest::ResponseCode::BAD,
            TRI_ERROR_HTTP_BAD_PARAMETER, "body must be an object");
          return;
        }

        auto modeSlice = slice.get("mode");
        if (!modeSlice.isString()) {
            generateError(rest::ResponseCode::BAD,
                TRI_ERROR_HTTP_BAD_PARAMETER, "mode must be a string");
            return;
        }

        auto newMode = ServerState::stringToMode(modeSlice.copyString());
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
            writeModeResult(newMode);
        } else {
            generateError(rest::ResponseCode::BAD,
                TRI_ERROR_HTTP_BAD_PARAMETER, "cannot set requested mode");
        }
    } else {
        generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
            TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    }
}