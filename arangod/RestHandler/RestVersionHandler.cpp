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
/// @author Achim Brandt
////////////////////////////////////////////////////////////////////////////////

#include "RestVersionHandler.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Rest/HttpRequest.h"
#include "Rest/Version.h"
#include "RestServer/ServerFeature.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoDB server
////////////////////////////////////////////////////////////////////////////////

RestVersionHandler::RestVersionHandler(GeneralRequest* request,
                                       GeneralResponse* response)
    : RestBaseHandler(request, response) {}

bool RestVersionHandler::isDirect() const { return true; }

RestHandler::status RestVersionHandler::execute() {
  VPackBuilder result;
  result.add(VPackValue(VPackValueType::Object));
  result.add("server", VPackValue("arango"));
  result.add("version", VPackValue(ARANGODB_VERSION));

  bool found;
  std::string const& detailsStr = _request->value("details", found);

  if (found && StringUtils::boolean(detailsStr)) {
    result.add("details", VPackValue(VPackValueType::Object));

    Version::getVPack(result);

    if (application_features::ApplicationServer::server != nullptr) {
      auto server = application_features::ApplicationServer::server
                        ->getFeature<ServerFeature>("Server");
      result.add("mode", VPackValue(server->operationModeString()));
    }

    result.close();
  }
  result.close();
  generateResult(rest::ResponseCode::OK, result.slice());
  return status::DONE;
}
