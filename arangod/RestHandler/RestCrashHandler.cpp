////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
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
/// @author Jure Bajic
////////////////////////////////////////////////////////////////////////////////

#include "RestCrashHandler.h"
#include <velocypack/Builder.h>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Inspection/VPack.h"
#include "RestServer/CrashHandlerFeature.h"
#include "Utils/ExecContext.h"
#include "Futures/Future.h"

namespace arangodb::crash_handler {

RestCrashHandler::RestCrashHandler(
    application_features::ApplicationServer& server, GeneralRequest* request,
    GeneralResponse* response)
    : RestBaseHandler(server, request, response) {}

futures::Future<futures::Unit> RestCrashHandler::executeAsync() {
  // Require admin access
  if (!ExecContext::current().isAdminUser()) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                  "you need admin rights for crash management operations");
    co_return;
  }

  auto& crashHandlerFeature = server().getFeature<CrashHandlerFeature>();
  if (!crashHandlerFeature.isEnabled()) {
    generateError(rest::ResponseCode::SERVICE_UNAVAILABLE, TRI_ERROR_DISABLED,
                  "crash handler feature is disabled");
    co_return;
  }

  auto const dumpManager = crashHandlerFeature.getDumpManager();
  if (dumpManager == nullptr) {
    generateError(rest::ResponseCode::SERVICE_UNAVAILABLE, TRI_ERROR_DISABLED,
                  "crash management is not ready yet");
    co_return;
  }

  std::vector<std::string> const& suffixes = _request->suffixes();
  if (suffixes.empty()) {
    // /_admin/crashes - list all crashes
    handleListCrashes(dumpManager);
  } else if (suffixes.size() == 1) {
    // /_admin/crashes/{id}
    auto const& crashId = suffixes[0];

    if (_request->requestType() == rest::RequestType::GET) {
      handleGetCrash(dumpManager, crashId);
    } else if (_request->requestType() == rest::RequestType::DELETE_REQ) {
      handleDeleteCrash(dumpManager, crashId);
    } else {
      generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                    TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    }
  } else {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND);
  }

  co_return;
}

void RestCrashHandler::handleListCrashes(
    std::shared_ptr<DumpManager> const& dumpManager) {
  if (_request->requestType() != rest::RequestType::GET) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    return;
  }

  auto crashes = dumpManager->listCrashes();

  VPackBuilder builder;
  velocypack::serialize(builder, crashes);

  generateOk(rest::ResponseCode::OK, builder.slice());
}

void RestCrashHandler::handleGetCrash(
    std::shared_ptr<DumpManager> const& dumpManager,
    std::string const& crashId) {
  auto contents = dumpManager->getCrashContents(crashId);

  if (contents.empty()) {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND,
                  "crash directory not found");
    return;
  }

  VPackBuilder responseBuilder;
  {
    VPackObjectBuilder guard(&responseBuilder);
    responseBuilder.add("crashId", VPackValue(crashId));

    responseBuilder.add(VPackValue("files"));
    velocypack::serialize(responseBuilder, contents);
  }

  generateOk(rest::ResponseCode::OK, responseBuilder.slice());
}

void RestCrashHandler::handleDeleteCrash(
    std::shared_ptr<DumpManager> const& dumpManager,
    std::string const& crashId) {
  bool deleted = dumpManager->deleteCrash(crashId);

  if (!deleted) {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND,
                  "crash directory not found");
    return;
  }

  VPackBuilder builder;
  {
    VPackObjectBuilder guard(&builder);
    builder.add("deleted", VPackValue(true));
    builder.add("crashId", VPackValue(crashId));
  }
  generateOk(rest::ResponseCode::OK, builder.slice());
}
}  // namespace arangodb::crash_handler
