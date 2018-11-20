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
#include "GeneralServer/AuthenticationFeature.h"
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
  } else if (suffixes.size() == 1 && suffixes[0] == "availability") {
    handleAvailability();
  } else {
    generateError(rest::ResponseCode::NOT_FOUND, 404);
  }
  return RestStatus::DONE;
}

void RestAdminServerHandler::writeModeResult(bool readOnly) {
  VPackBuilder builder;
  {
    VPackObjectBuilder b(&builder);
    builder.add("mode", VPackValue(readOnly ? "readonly" : "default")
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
  if (ReplicationFeature::INSTANCE != nullptr &&
      ReplicationFeature::INSTANCE->isActiveFailoverEnabled()) {
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

/// @brief simple availability check
/// this handler does not require authentication
/// it will return HTTP 200 in case the server is up and usable,
/// and not in read-only mode (or a follower in case of active failover)
/// will return HTTP 503 in case the server is starting, stopping, set
/// to read-only or a follower in case of active failover
void RestAdminServerHandler::handleAvailability() {
  if (_request->requestType() != rest::RequestType::GET) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
      TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    return;
  }
  
  bool available = false;
  switch (ServerState::mode()) {
    case ServerState::Mode::DEFAULT:
      available = !application_features::ApplicationServer::isStopping();
      break;
    case ServerState::Mode::MAINTENANCE: 
    case ServerState::Mode::REDIRECT:
    case ServerState::Mode::TRYAGAIN: 
    case ServerState::Mode::INVALID:
      TRI_ASSERT(!available);
      break;
  }
 
  if (!available) {
    // this will produce an HTTP 503 result
    generateError(rest::ResponseCode::SERVICE_UNAVAILABLE, TRI_ERROR_HTTP_SERVICE_UNAVAILABLE);
  } else {
    // this will produce an HTTP 200 result
    writeModeResult(ServerState::readOnly());
  }
}

void RestAdminServerHandler::handleMode() {
  auto const requestType = _request->requestType();
  if (requestType == rest::RequestType::GET) {
    writeModeResult(ServerState::readOnly());
  } else if (requestType == rest::RequestType::PUT) {
    
    AuthenticationFeature* af = AuthenticationFeature::instance();
    if (af->isActive() && !_request->user().empty()) {
      auth::Level lvl = auth::Level::NONE;
      if (af->userManager() != nullptr) {
        lvl = af->userManager()->databaseAuthLevel(_request->user(),
                                                   TRI_VOC_SYSTEM_DATABASE, /*configured*/true);
      } else {
        lvl = auth::Level::RW;
      }
      if (lvl < auth::Level::RW) {
        generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN);
        return;
      }
    }
    
    bool parseSuccess = false;
    VPackSlice slice = this->parseVPackBody(parseSuccess);
    if (!parseSuccess) {
      generateError(rest::ResponseCode::BAD,
                    TRI_ERROR_HTTP_BAD_PARAMETER, "invalid JSON");
      return;
    }
    
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
    
    
    Result res;
    if (modeSlice.compareString("readonly") == 0) {
      res = ServerState::instance()->propagateClusterReadOnly(true);
    } else if (modeSlice.compareString("default") == 0) {
      res = ServerState::instance()->propagateClusterReadOnly(false);
    } else {
      generateError(rest::ResponseCode::BAD,
                    TRI_ERROR_HTTP_BAD_PARAMETER, "mode invalid");
      return;
    }
    
    if (res.fail()) {
      generateError(rest::ResponseCode::BAD,
                    TRI_ERROR_HTTP_SERVER_ERROR, "couldn't set requested mode");
      LOG_TOPIC(ERR, Logger::FIXME) << "Couldn't set requested mode: " << res.errorMessage();
      return;
    }
    writeModeResult(ServerState::readOnly());
    
  } else {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
  }
}
